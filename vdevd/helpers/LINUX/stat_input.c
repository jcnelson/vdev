/*
 * stat_input - reads input properties
 * 
 * Forked from systemd/src/udev/udev-builtin-input_id.c on January 23, 2015.
 * (original from http://cgit.freedesktop.org/systemd/systemd/plain/src/udev/udev-builtin-input_id.c)
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/*
 * expose input properties via udev
 *
 * Copyright (C) 2009 Martin Pitt <martin.pitt@ubuntu.com>
 * Portions Copyright (C) 2004 David Zeuthen, <david@fubar.dk>
 * Copyright (C) 2011 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2014 David Herrmann <dh.herrmann@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "common.h"
#include <linux/limits.h>
#include <linux/input.h>

/* we must use this kernel-compatible implementation */
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

#define VDEV_INPUT_CLASS_KBD            0x1
#define VDEV_INPUT_CLASS_MOUSE          0x2
#define VDEV_INPUT_CLASS_JOYSTICK       0x3

int g_input_class = 0;

static inline int abs_size_mm(const struct input_absinfo *absinfo) {
   /* Resolution is defined to be in units/mm for ABS_X/Y */
   return (absinfo->maximum - absinfo->minimum) / absinfo->resolution;
}

static void extract_input_resolution( const char *devpath ) {
   
   char width[50], height[50];
   struct input_absinfo xabsinfo = {}, yabsinfo = {};
   
   int rc = 0;
   int fd = -1;

   fd = open(devpath, O_RDONLY|O_CLOEXEC);
   if (fd < 0) {
      rc = -errno;
      log_debug("failed to open '%s', errno = %d", devpath, rc );
      return;
   }

   if (ioctl(fd, EVIOCGABS(ABS_X), &xabsinfo) < 0 || ioctl(fd, EVIOCGABS(ABS_Y), &yabsinfo) < 0) {
      
      close( fd );
      return;
   }

   if (xabsinfo.resolution <= 0 || yabsinfo.resolution <= 0) {
      
      close( fd );
      return;
   }

   snprintf(width, sizeof(width), "%d", abs_size_mm(&xabsinfo));
   snprintf(height, sizeof(height), "%d", abs_size_mm(&yabsinfo));

   vdev_property_add( "VDEV_INPUT_WIDTH_MM", width );
   vdev_property_add( "VDEV_INPUT_HEIGHT_MM", height );
   
   close( fd );
}

/*
 * Read a capability attribute and return bitmask.
 * @param bitmask: Output array which has a length of bitmask_len
 */
static void get_cap_mask( char const* sysfs_cap_path, unsigned long *bitmask, size_t bitmask_len) {
   
   char text[4096];
   unsigned i;
   char* word;
   unsigned long val;
   int rc = 0;
   
   memset( text, 0, 4096 );
   
   int fd = open( sysfs_cap_path, O_RDONLY );
   if( fd < 0 ) {
      return;
   }
   
   rc = vdev_read_uninterrupted( fd, text, 4096 );
   close( fd );
   
   if( rc < 0 ) {
      
      log_debug("read('%s') errno = %d\n", sysfs_cap_path, rc );   
      return;
   }
   
   memset( bitmask, 0, bitmask_len * sizeof(unsigned long) );
   
   i = 0;
   
   while ((word = strrchr(text, ' ')) != NULL) {
      
      val = strtoul (word+1, NULL, 16);
      if (i < bitmask_len ) {
         bitmask[i] = val;
      }
      else {
         log_debug("ignoring %s block %lX which is larger than maximum length %zu", sysfs_cap_path, val, bitmask_len);
      }
      *word = '\0';
      ++i;
   }
   val = strtoul (text, NULL, 16);
   if (i < bitmask_len) {
      bitmask[i] = val;
   }
   else {
      log_debug("ignoring %s block %lX which is larger than maximum length %zu", sysfs_cap_path, val, bitmask_len);
   }
   
   log_debug("Bitmask path: %s, length: %zu, Text: '%s'", sysfs_cap_path, bitmask_len, text );
   for( unsigned int i = 0; i < bitmask_len; i++ ) {
      log_debug("Bitmask[%d] = %lX", i, bitmask[i]);
   }
}

/* pointer devices */
static void test_pointers (const unsigned long* bitmask_ev,
                           const unsigned long* bitmask_abs,
                           const unsigned long* bitmask_key,
                           const unsigned long* bitmask_rel ) {
   int is_mouse = 0;
   int is_touchpad = 0;

   if (!test_bit (EV_KEY, bitmask_ev)) {
      if (test_bit (EV_ABS, bitmask_ev) &&
         test_bit (ABS_X, bitmask_abs) &&
         test_bit (ABS_Y, bitmask_abs) &&
         test_bit (ABS_Z, bitmask_abs)) {
         
         vdev_property_add( "VDEV_INPUT_ACCELEROMETER", "1" );
      }
      return;
   }

   if (test_bit (EV_ABS, bitmask_ev) && test_bit (ABS_X, bitmask_abs) && test_bit (ABS_Y, bitmask_abs)) {

      if (test_bit (BTN_STYLUS, bitmask_key) || test_bit (BTN_TOOL_PEN, bitmask_key)) {
         
         vdev_property_add( "VDEV_INPUT_TABLET", "1" );
         
         g_input_class = VDEV_INPUT_CLASS_MOUSE;
      }
      
      else if (test_bit (BTN_TOOL_FINGER, bitmask_key) && !test_bit (BTN_TOOL_PEN, bitmask_key)) {
         
         is_touchpad = 1;
      }
      
      else if (test_bit (BTN_MOUSE, bitmask_key)) {
         /* This path is taken by VMware's USB mouse, which has
            * absolute axes, but no touch/pressure button. */
         
         is_mouse = 1;
      }
      
      else if (test_bit (BTN_TOUCH, bitmask_key)) {
         
         vdev_property_add( "VDEV_INPUT_TOUCHSCREEN", "1" );
         
         g_input_class = VDEV_INPUT_CLASS_MOUSE;
      }
      
      /* joysticks don't necessarily have to have buttons; e. g.
      * rudders/pedals are joystick-like, but buttonless; they have
      * other fancy axes */
      else if (test_bit (BTN_TRIGGER, bitmask_key) ||
               test_bit (BTN_A, bitmask_key) ||
               test_bit (BTN_1, bitmask_key) ||
               test_bit (ABS_RX, bitmask_abs) ||
               test_bit (ABS_RY, bitmask_abs) ||
               test_bit (ABS_RZ, bitmask_abs) ||
               test_bit (ABS_THROTTLE, bitmask_abs) ||
               test_bit (ABS_RUDDER, bitmask_abs) ||
               test_bit (ABS_WHEEL, bitmask_abs) ||
               test_bit (ABS_GAS, bitmask_abs) ||
               test_bit (ABS_BRAKE, bitmask_abs)) {
         
         vdev_property_add( "VDEV_INPUT_JOYSTICK", "1" );
         
         g_input_class = VDEV_INPUT_CLASS_JOYSTICK;
      }
   }

   if (test_bit (EV_REL, bitmask_ev) &&
      test_bit (REL_X, bitmask_rel) && test_bit (REL_Y, bitmask_rel) &&
      test_bit (BTN_MOUSE, bitmask_key)) {
       
      is_mouse = 1;
   }
   
   if (is_mouse) {
      
      vdev_property_add( "VDEV_INPUT_MOUSE", "1" );
      
      g_input_class = VDEV_INPUT_CLASS_MOUSE;
   }
   
   if( is_touchpad ) {
      
      vdev_property_add( "VDEV_INPUT_TOUCHPAD", "1" );
      
      g_input_class = VDEV_INPUT_CLASS_MOUSE;
   }
}

/* key like devices */
static void test_key (const unsigned long* bitmask_ev,
                      const unsigned long* bitmask_key ) {
   unsigned i;
   unsigned long found;
   unsigned long mask;

   /* do we have any KEY_* capability? */
   if (!test_bit (EV_KEY, bitmask_ev)) {
      log_debug("%s", "test_key: no EV_KEY capability");
      return;
   }

   /* only consider KEY_* here, not BTN_* */
   found = 0;
   for (i = 0; i < BTN_MISC/BITS_PER_LONG; ++i) {
      found |= bitmask_key[i];
      log_debug("test_key: checking bit block %lu for any keys; found=%i", (unsigned long)i*BITS_PER_LONG, found > 0);
   }
   
   /* If there are no keys in the lower block, check the higher block */
   if (!found) {
      for (i = KEY_OK; i < BTN_TRIGGER_HAPPY; ++i) {
         if (test_bit (i, bitmask_key)) {
            log_debug("test_key: Found key %x in high block", i);
            found = 1;
            break;
         }
      }
   }

   if (found > 0) {
      
      vdev_property_add( "VDEV_INPUT_KEY", "1" );
   }
   
   /* the first 32 bits are ESC, numbers, and Q to D; if we have all of
   * those, consider it a full keyboard; do not test KEY_RESERVED, though */
   mask = 0xFFFFFFFE;
   if ((bitmask_key[0] & mask) == mask) {
      
      vdev_property_add( "VDEV_INPUT_KEYBOARD", "1" );
      
      g_input_class = VDEV_INPUT_CLASS_KBD;
   }
}


void usage(char const* program_name ) {
   
   fprintf(stderr, "[ERROR] %s: Usage: %s /path/to/input/device/file\n", program_name, program_name);
   
}

// entry point 
// TODO: support using a sysfs device path directly
int main( int argc, char** argv ) {
   
   // look for the character device with VDEV_MAJOR and VDEV_MINOR 
   char capabilities_path[4096];
   char full_sysfs_path[4096];
   char subsystem[4096];
   char sysdev_path[4096];
   
   unsigned long bitmask_ev[NBITS(EV_MAX)];
   unsigned long bitmask_abs[NBITS(ABS_MAX)];
   unsigned long bitmask_key[NBITS(KEY_MAX)];
   unsigned long bitmask_rel[NBITS(REL_MAX)];
   
   struct stat sb;
   int rc = 0;
   char major[20];
   char minor[20];
   char* basename = NULL;
   
   if( argc != 2 ) {
      usage( argv[0] );
      exit(1);
   }
   
   rc = stat( argv[1], &sb );
   if( rc != 0 ) {
      
      rc = -errno;
      fprintf(stderr, "[ERROR] %s: stat('%s') errno = %d\n", argv[0], argv[1], rc );
      exit(2);
   }
   
   if( !S_ISCHR( sb.st_mode ) ) {
      
      fprintf(stderr, "[ERROR] %s: '%s' is not a character device file\n", argv[0], argv[1] );
      usage(argv[0]);
      exit(2);
   }
   
   sprintf( major, "%u", major(sb.st_rdev) );
   sprintf( minor, "%u", minor(sb.st_rdev) );
   
   static char const* caps[] = {
      "ev",
      "abs",
      "key",
      "rel",
      NULL
   };
   
   unsigned long *bitmasks[] = {
      bitmask_ev,
      bitmask_abs,
      bitmask_key,
      bitmask_rel,
      NULL
   };
   
   size_t bitmask_lens[] = {
      NBITS(EV_MAX),
      NBITS(ABS_MAX),
      NBITS(KEY_MAX),
      NBITS(REL_MAX),
      0
   };
   
   if( major == NULL || minor == NULL ) {
      // nothing to do here 
      exit(1);
   }
   
   // is this an input device?
   memset( sysdev_path, 0, 4096 );
   memset( subsystem, 0, 4096 );
   
   snprintf( sysdev_path, 4096, "/sys/dev/char/%s:%s/subsystem", major, minor );
   rc = readlink( sysdev_path, subsystem, 4096 );
   if( rc < 0 ) {
      fprintf(stderr, "[ERROR] %s: readlink('%s'): %s\n", argv[0], sysdev_path, strerror( rc ) );
      exit(1);
   }
   
   if( strcmp( subsystem + strlen(subsystem) - 5, "input" ) != 0 ) {
      
      // not an input device 
      exit(1);
   }
   
   // replaces ID_INPUT
   vdev_property_add( "VDEV_INPUT", "1" );
   
   memset( capabilities_path, 0, 4096 );
   
   snprintf(capabilities_path, 4095, "/sys/dev/char/%s:%s/device/capabilities", major, minor );
   
   // read each capability
   for( int i = 0; caps[i] != NULL; i++ ) {
      
      memset( full_sysfs_path, 0, 4096 );
      snprintf( full_sysfs_path, 4096, "%s/%s", capabilities_path, caps[i] );
      
      get_cap_mask( full_sysfs_path, bitmasks[i], bitmask_lens[i] );
   }
   
   // build up properties
   test_pointers( bitmask_ev, bitmask_abs, bitmask_key, bitmask_rel );
   test_key( bitmask_ev, bitmask_key );
   
   basename = rindex( argv[1], '/' ) + 1;
   
   if( strncmp( basename, "event", 5) == 0 ) {
      
      extract_input_resolution( argv[1] );
   }
   
   // state the input class too 
   switch( g_input_class ) {
      
      case VDEV_INPUT_CLASS_JOYSTICK: {
         
         vdev_property_add( "VDEV_INPUT_CLASS", "joystick" );
         break;
      }
      
      case VDEV_INPUT_CLASS_MOUSE: {
         
         vdev_property_add( "VDEV_INPUT_CLASS", "mouse" );
         break;
      }
      
      case VDEV_INPUT_CLASS_KBD: {
         
         vdev_property_add( "VDEV_INPUT_CLASS", "kbd" );
         break;
      }
   }
   
   vdev_property_print();
   vdev_property_free_all();
   
   return 0;
}
