from setuptools import setup, find_packages

setup(name='teleport-python-utils',
      version='0.0.1',
      description='Python Utilities for Teleport',
      license='Gnu Affero GPLv3',
      author='peer node',
      packages=find_packages(),
      include_package_data=True,
      zip_safe=False,
      scripts=['teleport_control', 'teleport_gui'],
      install_requires=['Flask_Babel', 'python-jsonrpc', 'Flask', 'sh', 'ecdsa', 'lockfile', 'requests'],
      entry_points={'console_scripts': []}
      )
