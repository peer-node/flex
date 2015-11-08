#!/usr/bin/env python

from setuptools import setup,find_packages
import sys
import os

setup(name='flex-python-utils',
      version='0.0.1',
      description='Python Utilities for Flex',
      license='Gnu Affero GPLv3',
      author='peer node',
      packages=find_packages(),
      include_package_data = True,
      zip_safe=False,
      scripts=['egt', 'cbf', 'cbt', 'daemonize',
               'flex_notary', 'flex_payment_processor', 'flex_cbt_plugin',
               'flexcontrol'],
      install_requires=['Flask_Babel', 'python-jsonrpc', 'Flask', 'sh',
                        'ecdsa', 'lockfile', 'requests'],
      entry_points = {'console_scripts': [
                            'flex_explorer = explorer.explorer:main',
                            'flex_interactive_payment_processor = '
                            'interactive_payment_processor'
                            '.interactive_payment_processor:main',
                            'flex_obk_plugin = '
                            'interactive_payment_processor'
                            '.obk_plugin:main',
                            'configure_flex = configuration.configure:main'
                                          ],
                      }
     )

if 'install' in sys.argv:
    executable = os.popen("which flexcontrol").read()
    if executable == "":
        print("Installation failed. Quitting\n")
        sys.exit(1)

    bindir = os.path.dirname(executable)

    os.chdir("..")

    from subprocess import Popen, PIPE

    print("Compiling executable...")
    autogen_process = Popen(['./autogen.sh'], stdout=PIPE, stderr=PIPE)
    autogen_output = autogen_process.stdout.read()
    configure_process = Popen(['./configure'], stdout=PIPE, stderr=PIPE)
    configure_output = configure_process.stdout.read()

    import multiprocessing
    num_cpus = multiprocessing.cpu_count()

    num_cpus_to_use = num_cpus - 2 if num_cpus > 3 else 1

    print("compiling with %s threads" % num_cpus_to_use)
    make_process = Popen(['make', '-j', str(num_cpus_to_use)], 
                         stdout=PIPE, stderr=PIPE)
    make_output = make_process.stdout.read()
    make_error = make_process.stderr.read()

    if os.path.exists(os.path.join("src", "flexd")):
        print("compilation successful. installing")
        import shutil
        for executable in ['flexd', 'flex-cli']:
            shutil.copy(os.path.join("src", executable), bindir)
    else:
        print("Compilation failed. Quitting\n")
        print(make_error)
        sys.exit(1)

    print("Installation successful")
