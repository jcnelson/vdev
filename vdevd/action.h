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

#ifndef _VDEV_ACTION_H_
#define _VDEV_ACTION_H_

#include "libvdev/util.h"
#include "libvdev/param.h"
#include "libvdev/config.h"

#include "device.h"

// action fields
#define VDEV_ACTION_NAME                "vdev-action"

#define VDEV_ACTION_NAME_EVENT          "event"
#define VDEV_ACTION_NAME_PATH           "path"
#define VDEV_ACTION_NAME_TYPE           "type"
#define VDEV_ACTION_NAME_RENAME         "rename_command"
#define VDEV_ACTION_NAME_COMMAND        "command"
#define VDEV_ACTION_NAME_HELPER         "helper"
#define VDEV_ACTION_NAME_ASYNC          "async"
#define VDEV_ACTION_NAME_IF_EXISTS      "if_exists"
#define VDEV_ACTION_NAME_OS_PREFIX      "OS_"
#define VDEV_ACTION_NAME_VAR_PREFIX     "VAR_"

#define VDEV_ACTION_EVENT_ADD           "add"
#define VDEV_ACTION_EVENT_REMOVE        "remove"
#define VDEV_ACTION_EVENT_CHANGE        "change"
#define VDEV_ACTION_EVENT_ANY           "any"

#define VDEV_ACTION_IF_EXISTS_ERROR     "error"
#define VDEV_ACTION_IF_EXISTS_MASK      "mask"
#define VDEV_ACTION_IF_EXISTS_RUN       "run"

#define VDEV_ACTION_DAEMONLET           "daemonlet"

enum vdev_action_if_exists {
	VDEV_IF_EXISTS_ERROR = 1,
	VDEV_IF_EXISTS_MASK,
	VDEV_IF_EXISTS_RUN
};

// vdev action to take on an event 
struct vdev_action {

	// action name 
	char *name;

	// what kind of request triggers this?
	vdev_device_request_t trigger;

	// device path to match on 
	char *path;
	regex_t path_regex;

	// device type to match on (block, char)
	bool has_type;
	char *type;

	// command to run to rename the matched path, if needed
	char *rename_command;

	// command to run once the device state change is processed.
	// can be dynamically generated from helper, below.
	char *command;

	// name of a helper to run once the device state change is processed (conflicts with command)
	char *helper;

	// whether or not to run this action in the system shell, or directly 
	bool use_shell;

	// OS-specific fields to match on
	vdev_params *dev_params;

	// helper-specific variables to export
	vdev_params *helper_vars;

	// synchronous or asynchronous 
	bool async;

	// how to handle the case where the device already exists 
	int if_exists;

	// is the action's command implemented as a daemonlet?  If so, hold onto its runtime state 
	bool is_daemonlet;
	int daemonlet_stdin;
	int daemonlet_stdout;
	pid_t daemonlet_pid;

	// action runtime statistics 
	uint64_t num_successful_calls;
	uint64_t cumulative_time_millis;
};

typedef struct vdev_action vdev_action;

C_LINKAGE_BEGIN
int vdev_action_init(struct vdev_action *act, vdev_device_request_t trigger,
		     char *path, char *command, char *helper, bool async);
int vdev_action_add_param(struct vdev_action *act, char const *name,
			  char const *value);
int vdev_action_free(struct vdev_action *act);
int vdev_action_free_all(struct vdev_action *act_list, size_t num_acts);

int vdev_action_load(struct vdev_config *config, char const *path,
		     struct vdev_action *act);

int vdev_action_load_all(struct vdev_config *config, struct vdev_action **acts,
			 size_t * num_acts);

int vdev_action_create_path(struct vdev_device_request *vreq,
			    struct vdev_action *acts, size_t num_acts,
			    char **path);
int vdev_action_run_commands(struct vdev_device_request *vreq,
			     struct vdev_action *acts, size_t num_acts,
			     bool exists);

int vdev_action_daemonlet_stop_all(struct vdev_action *actions,
				   size_t num_actions);

int vdev_action_log_benchmarks(struct vdev_action *action);

C_LINKAGE_END
#endif
