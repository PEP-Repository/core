#!/usr/bin/env python3
#
# SCRIPT to run on the "pull host" to copy data from a production environment to
# an acceptance environment, overwriting any existing acceptance data.
# See the README.md file for documentation.

# Configuration:  
SYSTEMCTL = 'sudo systemctl'
START_ACC_WATCHDOG  = f'{SYSTEMCTL} start docker-watchdog-acc'
STOP_ACC_WATCHDOG   = f'{SYSTEMCTL} stop  docker-watchdog-acc'
START_PROD_WATCHDOG = f'{SYSTEMCTL} start docker-watchdog'
STOP_PROD_WATCHDOG  = f'{SYSTEMCTL} stop  docker-watchdog'

import os
import sys
import time
import argparse
import subprocess

class Program:
    def __init__(self):
        pass

    def load_args(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('--clean', action='store_true',
                help='remove existing lists if necessary')
        parser.add_argument('--settling-time',
                type=int, default="60",
                help='number of seconds to wait to reassure '
                    'ourselves that we captured a momentarily unchanging '
                    '(and thus presumably consistent) state at production '
                    'before transferring it to acceptance.')
        parser.add_argument('--no-dry-run', action='store_true',
                help='not only make the lists, but actually change the '
                'acceptance environment')

        try:
            self.args = parser.parse_args()
        except SystemExit:
            sys.exit(37) # otherwise --help gives exit code 0
    
    def __call__(self):
        # Adapted from https://stackoverflow.com/a/41648790
        dirname = os.path.dirname(os.path.abspath(__file__))
        push_sh = os.path.join(dirname, 'push.sh')
        
        self.load_args()
        
        clean_flag = '--clean' if self.args.clean else ''

        self.execute_cmd(STOP_PROD_WATCHDOG)

        # phase I
        self.execute_cmd(f'{push_sh} I {clean_flag}')

        print(f"waiting {self.args.settling_time} before proceeding...") 
        time.sleep(self.args.settling_time)

        # phase II
        self.execute_cmd(f'{push_sh} II')

        self.execute_cmd(START_PROD_WATCHDOG)

        if not self.args.no_dry_run:
            print("Dry run completed successfully.")
            return
        
        print("Warning: modifying acceptance environment in 5 seconds..")
        time.sleep(5)

        self.execute_cmd(STOP_ACC_WATCHDOG)

        # phase III
        self.execute_cmd(f'{push_sh} III')

        time.sleep(5) # give the servers some time to restart

        self.execute_cmd(START_ACC_WATCHDOG)


    def execute_cmd(self, cmd):
        print(cmd)
        subprocess.run(cmd, check=True, shell=True)
        

def main():
    Program()()


def fatal(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(37)

main()
