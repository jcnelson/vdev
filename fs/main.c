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

#include "main.h"

// run! 
int main(int argc, char **argv)
{

	int rc = 0;
	pid_t pid = 0;
	struct vdevfs vdev;

	memset(&vdev, 0, sizeof(struct vdevfs));

	// set up global vdev state
	rc = vdevfs_init(&vdev, argc, argv);
	if (rc != 0) {

		vdev_error("vdevfs_init rc = %d\n", rc);

		exit(1);
	}
	// run!
	rc = vdevfs_main(&vdev, vdev.fuse_argc, vdev.fuse_argv);

	vdevfs_shutdown(&vdev);

	return rc;
}
