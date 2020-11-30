# Mock Sensor Tool

The mock-sensor-tool allows a user to overload sensor values retrieved from a
phosphor-hwmon or dbus-sensors instance at the kernel interface level.

The associated design document can be found here:
https://gerrit.openbmc-project.xyz/c/openbmc/docs/+/37869

## Usage
First, find the PID of the phosphor-hwmon or dbus-sensors instance to inject
mock values in to. For example:
systemctl (to list the names of all the services)
systemctl status xyz.openbmc_project.Hwmon@-ahb-apb-pwm\\x2dtacho\\x2dcontroller\\x401e786000.service
will return the PID of a phosphor-hwmon instance.

Then, run the mock-sensor-tool and input the PID when prompted to. After a
small delay, the tool will start and the user interface will instruct the user.

There is currently a temporarily "sleep" command that allows the user to put the
tool in the background (via ctrl+z and the bg command) and run other commands in
the BMC instance without the use of a multiplexer. While this solution isn't
ideal, with the time constraints of the internship in mind this serves as a
"band-aid" solution as putting the mock-sensor-tool in the background without
sleeping the user interface thread will cause linux to stop the overloading
thread as well (SIGTTIN)

## Other Considerations

Currently, all values that fit into a 32 bit long are supported, and all the
standard linux errno codes are supported as well:
https://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html

In the case that PTRACE_ATTACH is disabled on the machine, the parameter in the
file /proc/sys/kernel/yama/ptrace_scope must be set to 0 (these are the sames
permissions tools like strace use). More information can be found here:
https://www.kernel.org/doc/Documentation/security/Yama.txt