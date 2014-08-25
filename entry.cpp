/*
   loginfs: a FUSE filesystem to allow unprivileged users to access privileged files on UNIX-like systems.
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "entry.h"
#include "debug.h"

// hash a name/path
long loginfs_entry_name_hash( char const* name ) {
   locale loc;
   const collate<char>& coll = use_facet<collate<char> >(loc);
   return coll.hash( name, name + strlen(name) );
}

// join two paths, writing the result to dest if dest is not NULL.
// otherwise, allocate and return a buffer containing the joined paths.
char* loginfs_fullpath( char const* root, char const* path, char* dest ) {

   char delim = 0;
   int path_off = 0;
   
   int len = strlen(path) + strlen(root) + 2;
   
   if( strlen(root) > 0 ) {
      size_t root_delim_off = strlen(root) - 1;
      if( root[root_delim_off] != '/' && path[0] != '/' ) {
         len++;
         delim = '/';
      }
      else if( root[root_delim_off] == '/' && path[0] == '/' ) {
         path_off = 1;
      }
   }

   if( dest == NULL ) {
      dest = CALLOC_LIST( char, len );
   }
   
   memset(dest, 0, len);
   
   strcpy( dest, root );
   if( delim != 0 ) {
      dest[strlen(dest)] = '/';
   }
   strcat( dest, path + path_off );
   
   return dest;
}

// insert a child entry into an loginfs_entry_set
void loginfs_entry_set_insert( loginfs_entry_set* set, char const* name, struct loginfs_entry* child ) {
   long nh = loginfs_entry_name_hash( name );
   return loginfs_entry_set_insert_hash( set, nh, child );
}

// insert a child entry into an loginfs_entry_set
void loginfs_entry_set_insert_hash( loginfs_entry_set* set, long hash, struct loginfs_entry* child ) {
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).second == NULL ) {
         set->at(i).second = child;
         set->at(i).first = hash;
         return;
      }
   }

   loginfs_dirent dent( hash, child );
   set->push_back( dent );
}


// find a child entry in a loginfs_entry_set
struct loginfs_entry* loginfs_entry_set_find_name( loginfs_entry_set* set, char const* name ) {
   long nh = loginfs_entry_name_hash( name );
   return loginfs_entry_set_find_hash( set, nh );
}


// find a child entry in an fs_entry set
struct loginfs_entry* loginfs_entry_set_find_hash( loginfs_entry_set* set, long nh ) {
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         return set->at(i).second;
      }
   }
   return NULL;
}


// remove a child entry from an loginfs_entry_set
bool loginfs_entry_set_remove( loginfs_entry_set* set, char const* name ) {
   long nh = loginfs_entry_name_hash( name );
   return loginfs_entry_set_remove_hash( set, nh );
}


// remove a child entry from an loginfs_entry_set
bool loginfs_entry_set_remove_hash( loginfs_entry_set* set, long nh ) {
   bool removed = false;
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         // invalidate this
         set->at(i).second = NULL;
         set->at(i).first = 0;
         removed = true;
         break;
      }
   }

   return removed;
}


// replace an entry
bool loginfs_entry_set_replace( loginfs_entry_set* set, char const* name, struct loginfs_entry* replacement ) {
   long nh = loginfs_entry_name_hash( name );
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).first == nh ) {
         (*set)[i].second = replacement;
         return true;
      }
   }
   return false;
}


// count the number of entries in an loginfs_entry_set
unsigned int loginfs_entry_set_count( loginfs_entry_set* set ) {
   unsigned int ret = 0;
   for( unsigned int i = 0; i < set->size(); i++ ) {
      if( set->at(i).second != NULL )
         ret++;
   }
   return ret;
}

// dereference an iterator to an loginfs_entry_set member
struct loginfs_entry* loginfs_entry_set_get( loginfs_entry_set::iterator* itr ) {
   return (*itr)->second;
}

// dereference an iterator to an loginfs_entry_set member
long loginfs_entry_set_get_name_hash( loginfs_entry_set::iterator* itr ) {
   return (*itr)->first;
}


// lock a file for reading
int loginfs_entry_rlock2( struct loginfs_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_rdlock( &fent->lock );
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_rdlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// lock a file for writing
int loginfs_entry_wlock2( struct loginfs_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_wrlock( &fent->lock );
   if( fent->type == LOGINFS_ENTRY_TYPE_DEAD ) {
      return -ENOENT;
   }
   
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_wrlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// unlock a file
int loginfs_entry_unlock2( struct loginfs_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_unlock( &fent->lock );
   if( rc == 0 ) {
      if( _debug_locks ) {
         dbprintf( "%p: %s, from %s:%d\n", fent, fent->name, from_str, line_no );
      }
   }
   else {
      errorf("pthread_rwlock_unlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// lock a file handle for reading
int loginfs_file_handle_rlock( struct loginfs_file_handle* fh ) {
   return pthread_rwlock_rdlock( &fh->lock );
}

// lock a file handle for writing
int loginfs_file_handle_wlock( struct loginfs_file_handle* fh ) {
   return pthread_rwlock_wrlock( &fh->lock );
}

// unlock a file handle
int loginfs_file_handle_unlock( struct loginfs_file_handle* fh ) {
   return pthread_rwlock_unlock( &fh->lock );
}

// lock a directory handle for reading
int loginfs_dir_handle_rlock( struct loginfs_dir_handle* dh ) {
   return pthread_rwlock_rdlock( &dh->lock );
}

// lock a directory handle for writing
int loginfs_dir_handle_wlock( struct loginfs_dir_handle* dh ) {
   return pthread_rwlock_wrlock( &dh->lock );
}

// unlock a directory handle
int loginfs_dir_handle_unlock( struct loginfs_dir_handle* dh ) {
   return pthread_rwlock_unlock( &dh->lock );
}

// read-lock a filesystem core
int loginfs_core_rlock( struct loginfs_core* core ) {
   return pthread_rwlock_rdlock( &core->lock );
}

// write-lock a filesystem core
int loginfs_core_wlock( struct loginfs_core* core ) {
   return pthread_rwlock_wrlock( &core->lock );
}

// unlock a filesystem core
int loginfs_core_unlock( struct loginfs_core* core ) {
   return pthread_rwlock_unlock( &core->lock );
}

// run the eval function on cur_ent.
// prev_ent must be write-locked, in case cur_ent gets deleted.
// return the eval function's return code.
// if the eval function fails, both cur_ent and prev_ent will be unlocked
static int loginfs_entry_ent_eval( struct loginfs_entry* prev_ent, struct loginfs_entry* cur_ent, int (*ent_eval)( struct loginfs_entry*, void* ), void* cls ) {
   long name_hash = loginfs_entry_name_hash( cur_ent->name );
   char* name_dup = strdup( cur_ent->name );
   
   int eval_rc = (*ent_eval)( cur_ent, cls );
   if( eval_rc != 0 ) {
      
      dbprintf("ent_eval(%" PRIX64 " (%s)) rc = %d\n", cur_ent->file_id, name_dup, eval_rc );
      
      // cur_ent might not even exist anymore....
      if( cur_ent->type != LOGINFS_ENTRY_TYPE_DEAD ) {
         loginfs_entry_unlock( cur_ent );
         if( prev_ent != cur_ent && prev_ent != NULL ) {
            loginfs_entry_unlock( prev_ent );
         }
      }
      else {
         free( cur_ent );
         
         if( prev_ent ) {
            dbprintf("Remove %s from %s\n", name_dup, prev_ent->name );
            loginfs_entry_set_remove_hash( prev_ent->children, name_hash );
            loginfs_entry_unlock( prev_ent );
         }
      }
   }
   
   free( name_dup );
   
   return eval_rc;
}

// resolve an absolute path, running a given function on each entry as the path is walked
// returns the locked loginfs_entry at the end of the path on success
struct loginfs_entry* loginfs_entry_resolve_path_cls( struct loginfs_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err, int (*ent_eval)( struct loginfs_entry*, void* ), void* cls ) {
   
   // if this path ends in '/', then append a '.'
   char* fpath = NULL;
   if( strlen(path) == 0 ) {
      *err = -EINVAL;
      return NULL;
   }

   if( path[strlen(path)-1] == '/' ) {
      fpath = loginfs_fullpath( path, ".", NULL );
   }
   else {
      fpath = strdup( path );
   }

   char* tmp = NULL;

   char* name = strtok_r( fpath, "/", &tmp );
   while( name != NULL && strcmp(name, ".") == 0 ) {
      name = strtok_r( NULL, "/", &tmp );
   }

   if( name == NULL && writelock ) {
      loginfs_entry_wlock( core->root );
   }
   else {
      loginfs_entry_rlock( core->root );
   }

   if( core->root->link_count == 0 ) {
      // filesystem was nuked
      free( fpath );
      loginfs_entry_unlock( core->root );
      *err = -ENOENT;
      return NULL;
   }

   struct loginfs_entry* cur_ent = core->root;
   struct loginfs_entry* prev_ent = NULL;

   // run our evaluator on the root entry (which is already locked)
   if( ent_eval ) {
      int eval_rc = loginfs_entry_ent_eval( prev_ent, cur_ent, ent_eval, cls );
      if( eval_rc != 0 ) {
         *err = eval_rc;
         free( fpath );
         return NULL;
      }
   }
   
   do {
       
       // if this isn't a directory, then invalid path
       if( name != NULL && cur_ent->type != LOGINFS_ENTRY_TYPE_DIR ) {
         if( cur_ent->type != LOGINFS_ENTRY_TYPE_DEAD ) {
            *err = -ENOTDIR;
         }
         else {
            *err = -ENOENT;
         }

         free( fpath );
         loginfs_entry_unlock( cur_ent );

         return NULL;
      }

      // do we have permission to search this directory?
      if( cur_ent->type == LOGINFS_ENTRY_TYPE_DIR && !LOGINFS_ENTRY_IS_DIR_READABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {
         
         errorf("User %" PRIu64 " of group %" PRIu64 " cannot read directory %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );
         
         // the appropriate read flag is not set
         *err = -EACCES;
         free( fpath );
         loginfs_entry_unlock( cur_ent );

         return NULL;
      }
      
      if( name == NULL )
         break;

      // resolve next name
      prev_ent = cur_ent;
      if( name != NULL ) {
         cur_ent = loginfs_entry_set_find_name( prev_ent->children, name );
      }
      else {
         // out of path
         break;
      }
      
      // NOTE: we can safely check deletion_in_progress, since it only gets written once (and while the parent is write-locked)
      if( cur_ent == NULL || cur_ent->deletion_in_progress ) {
         // not found
         *err = -ENOENT;
         free( fpath );
         loginfs_entry_unlock( prev_ent );
         
         return NULL;
      }
      else {
         // next path name
         name = strtok_r( NULL, "/", &tmp );
         while( name != NULL && strcmp(name, ".") == 0 ) {
            name = strtok_r( NULL, "/", &tmp );
         }
         
         loginfs_entry_wlock( cur_ent );
         
         // before unlocking the previous ent, run our evaluator (if we have one)
         if( ent_eval ) {
            int eval_rc = loginfs_entry_ent_eval( prev_ent, cur_ent, ent_eval, cls );
            if( eval_rc != 0 ) {
               *err = eval_rc;
               free( fpath );
               return NULL;
            }
         }
         
         
         // If this is the last step of the path,
         // downgrade to a read lock if requested
         if( name == NULL && !writelock ) {
            loginfs_entry_unlock( cur_ent );
            loginfs_entry_rlock( cur_ent );
         }
         
         loginfs_entry_unlock( prev_ent );

         if( cur_ent->link_count == 0 || cur_ent->type == LOGINFS_ENTRY_TYPE_DEAD ) {
           // just got removed
           *err = -ENOENT;
           free( fpath );
           loginfs_entry_unlock( cur_ent );

           return NULL;
         }
      }
   } while( true );
   
   free( fpath );
   if( name == NULL ) {
      // ran out of path
      *err = 0;
      
      // check readability
      if( !LOGINFS_ENTRY_IS_READABLE( cur_ent->mode, cur_ent->owner, cur_ent->group, user, group ) ) {
         
         errorf("User %" PRIu64 " of group %" PRIu64 " cannot read file %" PRIX64 " owned by %" PRIu64 " in group %" PRIu64 "\n",
                user, group, cur_ent->file_id, cur_ent->owner, cur_ent->group );
         
         *err = -EACCES;
         loginfs_entry_unlock( cur_ent );
         return NULL;
      }
      
      return cur_ent;
   }
   else {
      // not a directory
      *err = -ENOTDIR;
      loginfs_entry_unlock( cur_ent );
      return NULL;
   }
}



// resolve an absolute path.
// returns the locked loginfs_entry at the end of the path on success
struct loginfs_entry* loginfs_entry_resolve_path( struct loginfs_core* core, char const* path, uint64_t user, uint64_t group, bool writelock, int* err ) {
   return loginfs_entry_resolve_path_cls( core, path, user, group, writelock, err, NULL, NULL );
}


