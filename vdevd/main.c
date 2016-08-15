/*
   vdev: a virtual device manager for *nix
   Copyright (C) 2015  Jude Nelson

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

// global vdev state
static struct vdev_state vdev;

// reload handler 
void
vdev_reload_sighup (int ignored)
{

  vdev_reload (&vdev);
}

// run! 
int
main (int argc, char **argv)
{

  int rc = 0;
  int coldplug_quiesce_pipe[2];
  bool is_child = false;
  bool is_parent = false;
  ssize_t nr = 0;

  int coldplug_finished_fd = -1;	// FD to write to once we finish flushing the initial device requests

  memset (&vdev, 0, sizeof (struct vdev_state));

  // ignore SIGPIPE from daemonlets 
  signal (SIGPIPE, SIG_IGN);

  // set up global vdev state
  rc = vdev_init (&vdev, argc, argv);
  // help called from command line -h or --help (-2)
  // short circuit with log/debug support
  if (vdev.config->help)
    {
      if (rc == -2)
	{

	  vdev_debug ("%s", "exiting: help called at command line\n");
	  // the shutdown and clean up should be redundant ?      
	  exit (0);
	}
    }

  if (rc != 0)
    {

      vdev_error ("vdev_init rc = %d\n", rc);

      vdev_shutdown (&vdev, false);
      exit (1);
    }
  // run the preseed command 
  rc = vdev_preseed_run (&vdev);
  if (rc != 0)
    {

      vdev_error ("vdev_preseed_run rc = %d\n", rc);

      vdev_shutdown (&vdev, false);
      exit (1);
    }
  // if we're going to daemonize, then redirect logging to the logfile
  if (!vdev.config->foreground)
    {

      // sanity check
      if (vdev.config->logfile_path == NULL)
	{

	  fprintf (stderr, "No logfile specified\n");

	  vdev_shutdown (&vdev, false);
	  exit (2);
	}
      // do we need to connect to syslog?
      if (strcmp (vdev.config->logfile_path, "syslog") == 0)
	{

	  vdev_debug ("%s", "Switching to syslog for messages\n");
	  vdev_enable_syslog ();
	}

      else
	{

	  // send to a specific logfile 
	  rc = vdev_log_redirect (vdev.config->logfile_path);
	  if (rc != 0)
	    {

	      vdev_error ("vdev_log_redirect('%s') rc = %d\n",
			  vdev.config->logfile_path, rc);

	      vdev_shutdown (&vdev, false);
	      exit (2);
	    }
	}

      if (!vdev.config->coldplug_only)
	{

	  // will become a daemon after handling coldplug. 
	  // set up a pipe between parent and child, so the child can 
	  // signal the parent when it's device queue has been emptied 
	  // (meaning the parent can safely exit).

	  rc = pipe (coldplug_quiesce_pipe);
	  if (rc != 0)
	    {

	      vdev_error ("pipe rc = %d\n", rc);
	      vdev_shutdown (&vdev, false);

	      exit (3);
	    }
	  // become a daemon
	  rc = vdev_daemonize ();
	  if (rc < 0)
	    {

	      vdev_error ("vdev_daemonize rc = %d\n", rc);
	      vdev_shutdown (&vdev, false);

	      exit (3);
	    }

	  if (rc == 0)
	    {

	      // child
	      close (coldplug_quiesce_pipe[0]);
	      coldplug_finished_fd = coldplug_quiesce_pipe[1];

	      // write a pidfile 
	      if (vdev.config->pidfile_path != NULL)
		{

		  rc = vdev_pidfile_write (vdev.config->pidfile_path);
		  if (rc != 0)
		    {

		      vdev_error
			("vdev_pidfile_write('%s') rc = %d\n",
			 vdev.config->pidfile_path, rc);

		      vdev_shutdown (&vdev, false);
		      exit (4);
		    }
		}
	      // set reload handler 
	      signal (SIGHUP, vdev_reload_sighup);

	      is_child = true;
	    }
	  else
	    {

	      // parent 
	      is_parent = true;
	      close (coldplug_quiesce_pipe[1]);
	    }
	}
    }

  if (!is_parent || vdev.config->foreground || vdev.config->coldplug_only)
    {

      // child, or foreground, or coldplug only.  start handling (coldplug) device events
      rc = vdev_start (&vdev);
      if (rc != 0)
	{

	  vdev_error ("vdev_backend_init rc = %d\n", rc);

	  vdev_stop (&vdev);
	  vdev_shutdown (&vdev, false);

	  // if child, and we're connected to the parent, then tell the parent to exit failure 
	  if (is_child && !vdev.config->foreground
	      && !vdev.config->coldplug_only)
	    {

	      write (coldplug_quiesce_pipe[1], &rc, sizeof (rc));
	      close (coldplug_quiesce_pipe[1]);
	    }

	  exit (5);
	}
      // main loop: get events from the OS and process them.
      // wake up the parent once we finish the coldplugged devices
      rc = vdev_main (&vdev, coldplug_finished_fd);
      if (rc != 0)
	{

	  vdev_error ("vdev_main rc = %d\n", rc);
	}
      // if only doing coldplug, find and remove all stale coldplug devices.
      // use the metadata directory to figure this out
      if (vdev.config->coldplug_only)
	{

	  rc = vdev_remove_unplugged_devices (&vdev);
	  if (rc != 0)
	    {

	      vdev_error ("vdev_remove_unplugged_devices() rc = %d\n", rc);
	    }
	}
      // quiesce requests
      vdev_stop (&vdev);

      // print benchmarks...
      vdev_debug ("%s", "Action benchmarks:\n");
      for (unsigned int i = 0; i < vdev.num_acts; i++)
	{

	  vdev_action_log_benchmarks (&vdev.acts[i]);
	}

      // clean up
      // keep the pidfile unless we're doing coldplug only (in which case don't touch it)
      vdev_shutdown (&vdev, !vdev.config->coldplug_only);

      return rc;
    }
  else
    {

      // parent process--wait for the child to finish processing the coldplugged devices, and exit.
      nr = read (coldplug_quiesce_pipe[0], &rc, sizeof (rc));

      if (nr < 0 || rc != 0)
	{

	  if (nr < 0)
	    {

	      vdev_error ("read(%d) rc = %zd\n",
			  coldplug_quiesce_pipe[0], nr);
	    }

	  vdev_error ("device quiesce failure, child rc = %d\n", rc);
	  exit (6);
	}

      printf ("parent: all initial devices processed\n");

      // clean up, but keep the pidfile
      vdev_shutdown (&vdev, false);

      close (coldplug_quiesce_pipe[0]);

      return 0;
    }
}
