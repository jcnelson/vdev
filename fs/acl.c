/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3 or later as 
   published by the Free Software Foundation. For the terms of this 
   license, see LICENSE.GPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU General
   Public License, but WITHOUT ANY WARRANTY; without even the implied 
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   Alternatively, you are free to use this program under the terms of the 
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or 
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include "acl.h"

#define INI_MAX_LINE 4096
#define INI_STOP_ON_FIRST_ERROR 1

#include "libvdev/ini.h"
#include "libvdev/match.h"
#include "libvdev/config.h"

SGLIB_DEFINE_VECTOR_PROTOTYPES(vdev_acl);
SGLIB_DEFINE_VECTOR_FUNCTIONS(vdev_acl);

// parse the ACL mode 
static int vdev_acl_parse_mode(mode_t * mode, char const *mode_str)
{

	char *tmp = NULL;

	*mode = ((mode_t) strtol(mode_str, &tmp, 8)) & 0777;

	if (*tmp != '\0') {
		return -EINVAL;
	} else {
		return 0;
	}
}

// get a passwd struct for a user.
// on success, fill in pwd and *pwd_buf (the caller must free *pwd_buf, but not pwd).
// return 0 on success
// return -EINVAL if any argument is NULL 
// return -ENOMEM on OOM 
// return -ENOENT if the username cnanot be loaded
int vdev_get_passwd(char const *username, struct passwd *pwd, char **pwd_buf)
{

	struct passwd *result = NULL;
	char *buf = NULL;
	int buf_len = 0;
	int rc = 0;

	if (pwd == NULL || username == NULL || pwd_buf == NULL) {
		return -EINVAL;
	}

	memset(pwd, 0, sizeof(struct passwd));

	buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_len <= 0) {
		buf_len = 65536;
	}

	buf = VDEV_CALLOC(char, buf_len);
	if (buf == NULL) {
		return -ENOMEM;
	}

	rc = getpwnam_r(username, pwd, buf, buf_len, &result);

	if (result == NULL) {

		if (rc == 0) {
			free(buf);
			return -ENOENT;
		} else {
			rc = -errno;
			free(buf);

			vdev_error("getpwnam_r(%s) errno = %d\n", username, rc);
			return rc;
		}
	}

	*pwd_buf = buf;

	// success!
	return rc;
}

// get a group struct for a group.
// on success, fill in grp and *grp_buf (the caller must free *grp_buf, but not grp).
// return 0 on success
// return -ENOMEM on OOM 
// return -EINVAL if any argument is NULL 
// return -ENOENT if the group cannot be found
int vdev_get_group(char const *groupname, struct group *grp, char **grp_buf)
{

	struct group *result = NULL;
	char *buf = NULL;
	int buf_len = 0;
	int rc = 0;

	if (grp == NULL || groupname == NULL || grp_buf == NULL) {
		return -EINVAL;
	}

	memset(grp, 0, sizeof(struct group));

	buf_len = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (buf_len <= 0) {
		buf_len = 65536;
	}

	buf = VDEV_CALLOC(char, buf_len);
	if (buf == NULL) {
		return -ENOMEM;
	}

	rc = getgrnam_r(groupname, grp, buf, buf_len, &result);

	if (result == NULL) {

		if (rc == 0) {
			free(buf);
			return -ENOENT;
		} else {
			rc = -errno;
			free(buf);

			vdev_error("getgrnam_r(%s) errno = %d\n", groupname,
				   rc);
			return rc;
		}
	}
	// success!
	return rc;
}

// parse a username or uid from a string.
// translate a username into a uid, if needed
// return 0 and set *uid on success 
// return negative if we failed to look up the corresponding user (see vdev_get_passwd)
// return negative on error 
int vdev_parse_uid(char const *uid_str, uid_t * uid)
{

	bool parsed = false;
	int rc = 0;

	if (uid_str == NULL || uid == NULL) {
		return -EINVAL;
	}

	*uid = (uid_t) vdev_parse_uint64(uid_str, &parsed);

	// not a number?
	if (!parsed) {

		// probably a username 
		char *pwd_buf = NULL;
		struct passwd pwd;

		// look up the uid...
		rc = vdev_get_passwd(uid_str, &pwd, &pwd_buf);
		if (rc != 0) {

			vdev_error("vdev_get_passwd(%s) rc = %d\n", uid_str,
				   rc);
			return rc;
		}

		*uid = pwd.pw_uid;

		free(pwd_buf);
	}

	return 0;
}

// parse a group name or GID from a string
// translate a group name into a gid, if needed
// return 0 and set gid on success 
// return negative if we failed to look up the corresponding group (see vdev_get_group)
// return negative on error
int vdev_parse_gid(char const *gid_str, gid_t * gid)
{

	bool parsed = false;
	int rc = 0;

	if (gid_str == NULL || gid == NULL) {
		return -EINVAL;
	}

	*gid = (gid_t) vdev_parse_uint64(gid_str, &parsed);

	// not a number?
	if (!parsed) {

		// probably a group name 
		char *grp_buf = NULL;
		struct group grp;

		// look up the gid...
		rc = vdev_get_group(gid_str, &grp, &grp_buf);
		if (rc != 0) {

			vdev_error("vdev_get_group(%s) rc = %d\n", gid_str, rc);
			return rc;
		}

		*gid = grp.gr_gid;

		free(grp_buf);
	}

	return 0;
}

// verify that a UID is valid
// return 0 on success
// return -ENOMEM on OOM 
// return -ENOENT on failure to look up the user
// return negative on error
int vdev_validate_uid(uid_t uid)
{

	struct passwd *result = NULL;
	char *buf = NULL;
	int buf_len = 0;
	int rc = 0;
	struct passwd pwd;

	memset(&pwd, 0, sizeof(struct passwd));

	buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_len <= 0) {
		buf_len = 65536;
	}

	buf = VDEV_CALLOC(char, buf_len);
	if (buf == NULL) {
		return -ENOMEM;
	}

	rc = getpwuid_r(uid, &pwd, buf, buf_len, &result);

	if (result == NULL) {

		if (rc == 0) {
			free(buf);
			return -ENOENT;
		} else {
			rc = -errno;
			free(buf);

			vdev_error("getpwuid_r(%d) errno = %d\n", uid, rc);
			return rc;
		}
	}

	if (uid != pwd.pw_uid) {
		// should *never* happen 
		rc = -EINVAL;
	}

	free(buf);

	return rc;
}

// verify that a GID is valid 
// return 0 on success
// return -ENOMEM on OOM 
// return -ENOENT if we couldn't find the corresponding group 
// return negative on other error
int vdev_validate_gid(gid_t gid)
{

	struct group *result = NULL;
	struct group grp;
	char *buf = NULL;
	int buf_len = 0;
	int rc = 0;

	memset(&grp, 0, sizeof(struct group));

	buf_len = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (buf_len <= 0) {
		buf_len = 65536;
	}

	buf = VDEV_CALLOC(char, buf_len);
	if (buf == NULL) {
		return -ENOMEM;
	}

	rc = getgrgid_r(gid, &grp, buf, buf_len, &result);

	if (result == NULL) {

		if (rc == 0) {
			free(buf);
			return -ENOENT;
		} else {
			rc = -errno;
			free(buf);

			vdev_error("getgrgid_r(%d) errno = %d\n", gid, rc);
			return rc;
		}
	}

	if (gid != grp.gr_gid) {
		rc = -EINVAL;
	}

	free(buf);

	return rc;
}

// callback from inih to parse an acl 
// return 1 on success
// return 0 on failure
static int vdev_acl_ini_parser(void *userdata, char const *section,
			       char const *name, char const *value)
{

	struct vdev_acl *acl = (struct vdev_acl *)userdata;

	// verify this is an acl section 
	if (strcmp(section, VDEV_ACL_NAME) != 0) {

		fprintf(stderr, "Invalid section '%s'\n", section);
		return 0;
	}

	if (strcmp(name, VDEV_ACL_NAME_UID) == 0) {

		// parse user or UID 
		uid_t uid = 0;
		int rc = vdev_parse_uid(value, &uid);

		if (rc != 0) {
			vdev_error("vdev_parse_uid(%s) rc = %d\n", value, rc);

			fprintf(stderr, "Invalid user/UID '%s'\n", value);
			return 0;
		}

		acl->has_uid = true;
		acl->uid = uid;
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_SETUID) == 0) {

		// parse user or UID 
		uid_t uid = 0;
		int rc = vdev_parse_uid(value, &uid);

		if (rc != 0) {
			vdev_error("vdev_parse_uid(%s) rc = %d\n", value, rc);

			fprintf(stderr, "Invalid user/UID '%s'\n", value);
			return 0;
		}

		acl->has_setuid = true;
		acl->setuid = uid;
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_GID) == 0) {

		// parse group or GID 
		gid_t gid = 0;
		int rc = vdev_parse_gid(value, &gid);

		if (rc != 0) {
			vdev_error("vdev_parse_gid(%s) rc = %d\n", value, rc);

			fprintf(stderr, "Invalid group/GID '%s'\n", value);
			return 0;
		}

		acl->has_gid = true;
		acl->gid = gid;
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_SETGID) == 0) {

		// parse group or GID 
		gid_t gid = 0;
		int rc = vdev_parse_gid(value, &gid);

		if (rc != 0) {
			vdev_error("vdev_parse_gid(%s) rc = %d\n", value, rc);

			fprintf(stderr, "Invalid group/GID '%s'\n", value);
			return 0;
		}

		acl->has_setgid = true;
		acl->setgid = gid;
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_SETMODE) == 0) {

		// parse mode 
		mode_t mode = 0;
		int rc = vdev_acl_parse_mode(&mode, value);

		if (rc != 0) {
			vdev_error("vdev_acl_parse_mode(%s) rc = %d\n", value,
				   rc);

			fprintf(stderr, "Invalid mode '%s'\n", value);
			return 0;
		}

		acl->has_setmode = true;
		acl->setmode = mode;
		return 1;
	}

	if (strcmp(name, VDEV_ACL_DEVICE_REGEX) == 0) {

		// parse and preserve this value 
		int rc =
		    vdev_match_regex_append(&acl->paths, &acl->regexes,
					    &acl->num_paths, value);

		if (rc != 0) {
			vdev_error("vdev_match_regex_append(%s) rc = %d\n",
				   value, rc);

			fprintf(stderr, "Invalid regex '%s'\n", value);
			return 0;
		}

		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_PROC_PATH) == 0) {

		// preserve this value 
		if (acl->proc_path != NULL) {

			fprintf(stderr, "Duplicate process path '%s'\n", value);
			return 0;
		}

		acl->has_proc = true;
		acl->proc_path = vdev_strdup_or_null(value);
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_PROC_PREDICATE) == 0) {

		// preserve this value 
		if (acl->proc_predicate_cmd != NULL) {

			fprintf(stderr, "Duplicate predicate '%s'\n", value);
			return 0;
		}

		acl->has_proc = true;
		acl->proc_predicate_cmd = vdev_strdup_or_null(value);
		return 1;
	}

	if (strcmp(name, VDEV_ACL_NAME_PROC_INODE) == 0) {

		// preserve this value 
		ino_t inode = 0;
		bool success = false;

		inode = vdev_parse_uint64(value, &success);
		if (!success) {

			fprintf(stderr, "Failed to parse inode '%s'\n", value);
			return 0;
		}

		acl->has_proc = true;
		acl->proc_inode = inode;
		return 1;
	}

	fprintf(stderr, "Unrecognized field '%s'\n", name);
	return 0;
}

// acl sanity check 
int vdev_acl_sanity_check(struct vdev_acl *acl)
{

	int rc = 0;

	if (acl->has_gid) {

		rc = vdev_validate_gid(acl->gid);
		if (rc != 0) {

			fprintf(stderr, "Invalid GID %d\n", acl->gid);
			return rc;
		}
	}

	if (acl->has_setgid) {

		rc = vdev_validate_gid(acl->gid);
		if (rc != 0) {

			fprintf(stderr, "Invalid set-GID %d\n", acl->setgid);
			return rc;
		}
	}

	if (acl->has_uid) {

		rc = vdev_validate_uid(acl->uid);
		if (rc != 0) {

			fprintf(stderr, "Invalid UID %d\n", acl->uid);
			return rc;
		}
	}

	if (acl->has_setuid) {

		rc = vdev_validate_uid(acl->uid);
		if (rc != 0) {

			fprintf(stderr, "Invalid set-UID %d\n", acl->setuid);
			return rc;
		}
	}

	return rc;
}

// initialize an acl 
int vdev_acl_init(struct vdev_acl *acl)
{
	memset(acl, 0, sizeof(struct vdev_acl));
	return 0;
}

// free an acl 
int vdev_acl_free(struct vdev_acl *acl)
{

	if (acl->num_paths > 0) {

		vdev_match_regexes_free(acl->paths, acl->regexes,
					acl->num_paths);
	}

	if (acl->proc_path != NULL) {
		free(acl->proc_path);
		acl->proc_path = NULL;
	}

	return 0;
}

// load an ACL from a file handle
int vdev_acl_load_file(FILE * file, struct vdev_acl *acl)
{

	int rc = 0;

	vdev_acl_init(acl);

	rc = ini_parse_file(file, vdev_acl_ini_parser, acl);
	if (rc != 0) {
		vdev_error("ini_parse_file(ACL) rc = %d\n", rc);
		return rc;
	}
	// sanity check 
	rc = vdev_acl_sanity_check(acl);
	if (rc != 0) {

		vdev_error("vdev_acl_sanity_check rc = %d\n", rc);

		vdev_acl_free(acl);
		memset(acl, 0, sizeof(struct vdev_acl));
		return rc;
	}

	return rc;
}

// load an ACL from a file, given the path
int vdev_acl_load(char const *path, struct vdev_acl *acl)
{

	int rc = 0;
	FILE *f = NULL;

	f = fopen(path, "r");
	if (f == NULL) {

		rc = -errno;
		vdev_error("fopen(%s) errno = %d\n", path, rc);
		return rc;
	}

	rc = vdev_acl_load_file(f, acl);

	fclose(f);

	if (rc != 0) {
		vdev_error("vdev_acl_load_file(%s) rc = %d\n", path, rc);
	}

	return rc;
}

// free a vector of acls
static void vdev_acl_free_vector(struct sglib_vdev_acl_vector *acls)
{

	for (unsigned long i = 0; i < sglib_vdev_acl_vector_size(acls); i++) {

		struct vdev_acl *acl = sglib_vdev_acl_vector_at_ref(acls, i);

		vdev_acl_free(acl);
	}

	sglib_vdev_acl_vector_clear(acls);
}

// free a C-style list of acls (including the list itself)
int vdev_acl_free_all(struct vdev_acl *acl_list, size_t num_acls)
{

	int rc = 0;

	for (unsigned int i = 0; i < num_acls; i++) {

		rc = vdev_acl_free(&acl_list[i]);
		if (rc != 0) {

			return rc;
		}
	}

	free(acl_list);

	return rc;
}

// loader for an acl 
int vdev_acl_loader(char const *fp, void *cls)
{

	struct vdev_acl acl;
	int rc = 0;
	struct stat sb;

	struct sglib_vdev_acl_vector *acls =
	    (struct sglib_vdev_acl_vector *)cls;

	// skip if not a regular file 
	rc = stat(fp, &sb);
	if (rc != 0) {

		rc = -errno;
		vdev_error("stat(%s) rc = %d\n", fp, rc);
		return rc;
	}

	if (!S_ISREG(sb.st_mode)) {

		return 0;
	}

	vdev_debug("Load ACL %s\n", fp);

	memset(&acl, 0, sizeof(struct vdev_acl));

	rc = vdev_acl_load(fp, &acl);
	if (rc != 0) {

		vdev_error("vdev_acl_load(%s) rc = %d\n", fp, rc);
		return rc;
	}
	// save this acl 
	rc = sglib_vdev_acl_vector_push_back(acls, acl);
	if (rc != 0) {

		// OOM 
		vdev_acl_free(&acl);
		return rc;
	}

	return 0;
}

// load all ACLs from a directory, in lexographic order 
// return 0 on success, negative on error
int vdev_acl_load_all(char const *dir_path, struct vdev_acl **ret_acls,
		      size_t * ret_num_acls)
{

	int rc = 0;
	struct sglib_vdev_acl_vector acls;

	sglib_vdev_acl_vector_init(&acls);

	rc = vdev_load_all(dir_path, vdev_acl_loader, &acls);

	if (rc != 0) {

		vdev_acl_free_vector(&acls);
		return rc;
	} else {

		if (sglib_vdev_acl_vector_size(&acls) == 0) {

			// nothing 
			*ret_acls = NULL;
			*ret_num_acls = 0;
		} else {

			// extract values
			unsigned long len = 0;

			sglib_vdev_acl_vector_yoink(&acls, ret_acls, &len);

			*ret_num_acls = len;
		}
	}

	return 0;
}

// modify a stat buffer to apply a user access control list, if the acl says so
int vdev_acl_do_set_user(struct vdev_acl *acl, uid_t caller_uid,
			 struct stat *sb)
{

	if (acl->has_setuid && acl->has_uid) {

		// change the UID of this path
		if (acl->uid == caller_uid) {
			sb->st_uid = acl->setuid;
		}
	}

	return 0;
}

// modify a stat buffer to apply a group access control list 
int vdev_acl_do_set_group(struct vdev_acl *acl, gid_t caller_gid,
			  struct stat *sb)
{

	if (acl->has_setgid && acl->has_gid) {

		// change the GID of this path
		if (acl->gid == caller_gid) {
			sb->st_gid = acl->setgid;
		}
	}

	return 0;
}

// modify a stat buffer to apply the mode
int vdev_acl_do_set_mode(struct vdev_acl *acl, struct stat *sb)
{

	if (acl->has_setmode) {

		// clear permission bits 
		sb->st_mode &= ~(0777);

		// set permission bits 
		sb->st_mode |= acl->setmode;
	}

	return 0;
}

// evaluate a predicate command, with the appropriate environment variables set.
// on success, fill in the exit code.
// return 0 on success
// return negative on error 
int vdev_acl_run_predicate(struct vdev_acl *acl, struct pstat *ps,
			   uid_t caller_uid, gid_t caller_gid, int *exit_code)
{

	int exit_status = 0;
	char *cmd_buf = NULL;
	char env_buf[3][100];
	char *predicate_env[4];
	int rc = 0;

	// build the command with the apporpriate environment variables:
	// * VDEV_GID: the gid of the calling process
	// * VDEV_UID: the uid of the calling process
	// * VDEV_PID: the pid of the calling process

	sprintf(env_buf[0], "VDEV_UID=%u", caller_uid);
	sprintf(env_buf[1], "VDEV_GID=%u", caller_gid);
	sprintf(env_buf[2], "VDEV_PID=%u", pstat_get_pid(ps));

	predicate_env[0] = env_buf[0];
	predicate_env[1] = env_buf[1];
	predicate_env[2] = env_buf[2];
	predicate_env[3] = NULL;

	rc = vdev_subprocess(cmd_buf, predicate_env, NULL, 0, -1, &exit_status,
			     true);

	if (rc != 0) {

		vdev_error("vdev_subprocess('%s') rc = %d\n", cmd_buf, rc);

		free(cmd_buf);
		return rc;
	}

	*exit_code = exit_status;

	free(cmd_buf);

	return 0;
}

// check that the caller PID is matched by the given ACL.
// every process match criterion must be satisfied.
// return 1 if all ACL criteria match
// return 0 if at least one ACL criterion does not match 
// return negative on error
int vdev_acl_match_process(struct vdev_acl *acl, struct pstat *ps,
			   uid_t caller_uid, gid_t caller_gid)
{

	int rc = 0;
	char path[PATH_MAX + 1];
	struct stat sb;

	if (!acl->has_proc) {
		// applies to anyone 
		return 1;
	}

	if (acl->proc_path != NULL || acl->has_proc_inode) {

		if (acl->proc_path != NULL) {

			pstat_get_path(ps, path);
			if (strcmp(acl->proc_path, path) != 0) {

				// doesn't match 
				return 0;
			}
		}

		if (acl->has_proc_inode) {

			pstat_get_stat(ps, &sb);
			if (acl->proc_inode != sb.st_ino) {

				// doesn't match 
				return 0;
			}
		}
	}
	// filter on a given PID list generator?
	if (acl->proc_predicate_cmd != NULL) {

		// run the command and use the exit code to see if 
		// this ACL applies to the calling process
		int exit_status = 0;

		rc = vdev_acl_run_predicate(acl, ps, caller_uid, caller_gid,
					    &exit_status);
		if (rc != 0) {

			vdev_error("vdev_acl_run_predicate('%s') rc = %d\n",
				   acl->proc_predicate_cmd, rc);
			return rc;
		}
		// exit status follows shell-like semantics:
		// 0 indicates "yes, this applies."
		// non-zero indicates "no, this does not apply"

		if (exit_status == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	// all checks match
	return 1;
}

// given a list of access control lists, find the index of the first one that applies to the given caller and path. 
// return >= 0 with the index
// return num_acls if not found
// return negative on error
int vdev_acl_find_next(char const *path, struct pstat *caller_proc,
		       uid_t caller_uid, gid_t caller_gid,
		       struct vdev_acl *acls, size_t num_acls)
{

	int rc = 0;
	bool found = false;
	int idx = 0;

	for (unsigned int i = 0; i < num_acls; i++) {

		rc = 0;

		// match UID?
		if (acls[i].has_uid && acls[i].uid != caller_uid) {
			// nope 
			continue;
		}
		// match GID?
		if (acls[i].has_gid && acls[i].gid != caller_gid) {
			// nope 
			continue;
		}
		// match path?
		if (acls[i].num_paths > 0) {

			rc = vdev_match_first_regex(path, acls[i].regexes,
						    acls[i].num_paths);

			if (rc >= (signed)acls[i].num_paths) {
				// no match
				continue;
			}
		}

		if (rc < 0) {

			vdev_error("vdev_match_first_regex(%s) rc = %d\n", path,
				   rc);
			break;
		}
		// match process?  Do this last, since it can be expensive 
		rc = vdev_acl_match_process(&acls[i], caller_proc, caller_uid,
					    caller_gid);
		if (rc == 0) {
			// no match 
			continue;
		}

		if (rc < 0) {

			// error...
			vdev_error("vdev_acl_match_process(%d) rc = %d\n",
				   pstat_get_pid(caller_proc), rc);
			break;
		}
		// success!
		found = true;
		idx = i;
		rc = 0;
		break;
	}

	if (found) {
		return idx;
	} else {
		if (rc >= 0) {
			return num_acls;
		} else {
			return rc;
		}
	}
}

// apply the acl to the stat buf, filtering on caller uid, gid, and process information
int vdev_acl_apply(struct vdev_acl *acl, uid_t caller_uid, gid_t caller_gid,
		   struct stat *sb)
{

	// set user, group, mode (if given)
	vdev_acl_do_set_user(acl, caller_uid, sb);
	vdev_acl_do_set_group(acl, caller_gid, sb);
	vdev_acl_do_set_mode(acl, sb);

	return 0;
}

// go through the list of acls and apply any modifications to the given stat buffer
// return 1 if at least one ACL matches
// return 0 if there are no matches (i.e. this device node should be hidden)
// return negative on error 
int vdev_acl_apply_all(struct vdev_config *config, struct vdev_acl *acls,
		       size_t num_acls, char const *path,
		       struct pstat *caller_proc, uid_t caller_uid,
		       gid_t caller_gid, struct stat *sb)
{

	int rc = 0;
	int acl_offset = 0;
	int i = 0;
	bool found = false;

	// special case: if there are no ACLs, then follow the config default policy
	if (num_acls == 0) {
		return config->default_policy;
	}

	while (acl_offset < (signed)num_acls) {

		// find the next acl 
		rc = vdev_acl_find_next(path, caller_proc, caller_uid,
					caller_gid, acls + acl_offset,
					num_acls - acl_offset);

		if (rc == (signed)(num_acls - acl_offset)) {

			// not found 
			rc = 0;
			break;
		} else if (rc < 0) {

			vdev_error
			    ("vdev_acl_find_next(%s, offset = %d) rc = %d\n",
			     path, acl_offset, rc);
			break;
		} else {

			// matched! advance offset to next acl
			i = acl_offset + rc;
			acl_offset += rc + 1;
			found = true;

			// apply the ACL 
			rc = vdev_acl_apply(&acls[i], caller_uid, caller_gid,
					    sb);
			if (rc != 0) {

				vdev_error
				    ("vdev_acl_apply(%s, offset = %d) rc = %d\n",
				     path, acl_offset, rc);
				break;
			}
		}
	}

	if (rc == 0) {
		// no error reported 
		if (found) {
			rc = 1;
		}
	}

	return rc;
}
