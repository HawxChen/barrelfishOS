##########################################################################
# Copyright (c) 2012-2016 ETH Zurich.
# All rights reserved.
#
# This file is distributed under the terms in the attached LICENSE file.
# If you do not find this file, copies can be found by writing to:
# ETH Zurich D-INFK, Universitaetstr 6, CH-8092 Zurich. Attn: Systems Group.
##########################################################################

import os, signal, tempfile, subprocess, shutil, time
import debug, machines
from machines import Machine

FVP_PATH = '/home/netos/tools/DS-5_v5.24.0/bin'
FVP_LICENSE = '8224@sgv-license-01.ethz.ch'
FVP_START_TIMEOUT = 5 # in seconds
IMAGE_NAME="armv7_a9ve_image"

class FVPMachineBase(Machine):
    def __init__(self, options):
        super(FVPMachineBase, self).__init__(options)
        self.child = None
        self.telnet = None
        self.tftp_dir = None
        self.options = options

    def get_buildall_target(self):
        return "VExpressEMM-A9"

    def get_coreids(self):
        return range(0, self.get_ncores())

    def get_tickrate(self):
        return None

    def get_boot_timeout(self):
        return 120

    def get_test_timeout(self):
        # 15 mins
        return 15 * 60

    def get_machine_name(self):
        return self.name

    def force_write(self, consolectrl):
        pass

    def get_tftp_dir(self):
        if self.tftp_dir is None:
            debug.verbose('Creating temporary directory for FVP files')
            self.tftp_dir = tempfile.mkdtemp(prefix='harness_fvp_')
            debug.verbose('FVP install directory is %s' % self.tftp_dir)
        return self.tftp_dir

    # Use menu.lst in hake/menu.lst.arm_fvp
    def _write_menu_lst(self, data, path):
        pass

    def set_bootmodules(self, modules):
        pass

    def lock(self):
        pass

    def unlock(self):
        pass

    def setup(self, builddir=None):
        self.builddir = builddir

    def _get_cmdline(self):
        raise NotImplementedError

    def get_kernel_args(self):
        # Fixed virtual platform has 100MHz clock that is not discoverable
        return [ "periphclk=100000000", "consolePort=0" ]

    def _kill_child(self):
        # terminate child if running
        if self.child:
            try:
                os.kill(self.child.pid, signal.SIGTERM)
            except OSError, e:
                debug.verbose("Caught OSError trying to kill child: %r" % e)
            except Exception, e:
                debug.verbose("Caught exception trying to kill child: %r" % e)
            try:
                self.child.wait()
            except Exception, e:
                debug.verbose(
                    "Caught exception while waiting for child: %r" % e)
            self.child = None

    def reboot(self):
        self._kill_child()
        cmd = self._get_cmdline()
        debug.verbose('starting "%s" in FVP:reboot' % ' '.join(cmd))
        devnull = open('/dev/null', 'w')
        import os
        env = dict(os.environ)
        env['ARMLMD_LICENSE_FILE'] = FVP_LICENSE
        self.child = \
            subprocess.Popen(cmd, stdout=subprocess.PIPE,
                             stderr=devnull, env=env)
        time.sleep(FVP_START_TIMEOUT)

    def shutdown(self):
        debug.verbose('FVP:shutdown requested');
        debug.verbose('terminating FVP')
        if not self.child is None:
            try:
                self.child.terminate()
            except OSError, e:
                debug.verbose("Error when trying to terminate FVP: %r" % e)
        debug.verbose('closing telnet connection')
        if not self.telnet is None:
            self.output.close()
            self.telnet.close()
        # try to cleanup tftp tree if needed
        if self.tftp_dir and os.path.isdir(self.tftp_dir):
            shutil.rmtree(self.tftp_dir, ignore_errors=True)
        self.tftp_dir = None

    def get_output(self):
        # wait a bit to give FVP time to listen for a telnet connection
        if self.child.poll() != None: # Check if child is down
            print 'FVP is down, return code is %d' % self.child.returncode
            return None
        # use telnetlib
        import telnetlib
        self.telnet_connected = False
        while not self.telnet_connected:
            try:
                self.telnet = telnetlib.Telnet("localhost", self.telnet_port)
                self.telnet_connected = True
                self.output = self.telnet.get_socket().makefile()
            except IOError, e:
                errno, msg = e
                if errno != 111: # connection refused
                    debug.error("telnet: %s [%d]" % (msg, errno))
                else:
                    self.telnet_connected = False
            time.sleep(FVP_START_TIMEOUT)

        return self.output

class FVPMachineARMv7(FVPMachineBase):
    def get_bootarch(self):
        return 'armv7'

    def get_platform(self):
        return 'a9ve'

    def set_bootmodules(self, modules):
        # store path to kernel for _get_cmdline to use
        self.kernel_img = os.path.join(self.options.buildbase,
                                       self.options.builds[0].name,
                                       IMAGE_NAME)

        # write menu.lst
        path = os.path.join(self.get_tftp_dir(), 'menu.lst')
        self._write_menu_lst(modules.get_menu_data('/'), path)

@machines.add_machine
class FVPMachineARMv7SingleCore(FVPMachineARMv7):
    name = 'armv7_fvp'

    def get_ncores(self):
        return 1

    def get_cores_per_socket(self):
        return 1

    def setup(self, builddir=None):
        self.builddir = builddir

    def get_free_port(self):
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 0))
        # extract port from addrinfo
        self.telnet_port = s.getsockname()[1]
        s.close()

    def _write_menu_lst(self, data, path):
        debug.verbose('writing %s' % path)
        debug.debug(data)
        f = open(path, 'w')
        f.write(data)
        # TODO: provide mmap properly somehwere (machine data?)
        # The FVP simulates 4GB of RAM, 2GB of which is in the 32-bit address space.
        #        start       size       id
        f.write("mmap map  0x80000000  0x40000000 1\n")
        f.write("mmap map  0xC0000000  0x40000000 1\n")
        f.write("mmap map 0x880000000  0x80000000 1\n")
        f.close()

    def set_bootmodules(self, modules):
        super(FVPMachineARMv7SingleCore, self).set_bootmodules(modules)
        debug.verbose("writing menu.lst in build directory")
        menulst_fullpath = os.path.join(self.builddir,
                "platforms", "arm", "menu.lst.armv7_a9ve")
        debug.verbose("writing menu.lst in build directory: %s" %
                menulst_fullpath)
        self._write_menu_lst(modules.get_menu_data("/"), menulst_fullpath)
        debug.verbose("building proper FVP image")
        debug.checkcmd(["make", IMAGE_NAME], cwd=self.builddir)

    def _get_cmdline(self):
        self.get_free_port()

        return [os.path.join(FVP_PATH, "FVP_VE_Cortex-A9x1"), 
                # Don't try to pop an LCD window up
                "-C", "motherboard.vis.disable_visualisation=1",
                # Don't start a telnet xterm
                "-C", "motherboard.terminal_0.start_telnet=0",
                "-C", "motherboard.terminal_0.start_port=%d"%self.telnet_port,
                self.kernel_img]
