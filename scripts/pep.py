import subprocess
import concurrent.futures
import io

class PepError(Exception):
    """Error for calls to PEP"""

    def __init__(self, cmd, returncode=None,
                   stdout=None, stderr=None):
        self.cmd = cmd
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.message = 'Error for command %s' % cmd
        if returncode is not None:
            self.message += ('\nExit code: %d' % returncode)
        if stdout is not None:
            self.message += ('\nstdout:\n%s' % stdout)
        if stderr is not None:
            self.message += ('\nstderr:\n%s' % stderr)
        super().__init__(self.message)


class Pep:
    """Wrapper for `pepcli`, the command line PEP client"""

    def __init__(self, token,
            pepcli_path="/app/pepcli", working_dir="/config"):
        self.pepcli_path = pepcli_path
        self.working_dir = working_dir
        self.token = token

    def __get_base_cmd(self):
        return [self.pepcli_path,
                '--client-working-directory', self.working_dir,
                '--oauth-token', self.token]

    def run(self, args):
        cmd = self.__get_base_cmd() + args
        popen = subprocess.Popen(cmd, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stream = PepOStream(popen, cmd)
        stdout, _ = stream.wait()
        return stdout

    def __store_build_args(self, column,
                           participant=None, short_pseudonym=None,
                           input_path=None, data=None):

        if ((participant is None and short_pseudonym is None)
                or (participant is not None
                    and short_pseudonym is not None)):
            raise ValueError('Either participant OR short_pseudonym should be '
                             'specified')
        if ((input_path is None and data is None) or
                (input_path is not None and data is not None)):
            raise ValueError('Either input_path OR data should be specified')

        participant_args = (['--participant', participant]
                            if participant is not None
                            else ['--short-pseudonym', short_pseudonym])
        data_args = (['--input-path', input_path]
                     if input_path is not None
                     else ['--data', data])

        return (['store', '--column', column]
                + participant_args + data_args)

    def store(self, column,
              participant=None, short_pseudonym=None,
              input_path=None, data=None):
        self.run(self.__store_build_args(
            column, participant, short_pseudonym, input_path, data))

    def store_stream(self, column,
                     participant=None, short_pseudonym=None,
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE):
        cmd = self.__get_base_cmd() + self.__store_build_args(
            column,  participant, short_pseudonym, input_path='-')
        popen = subprocess.Popen(cmd, stdin=stdin, stdout=stdout,
                                 stderr=stderr)
        return PepOStream(popen, cmd)

    def pull(self, outputPath, columns=[], column_groups=[],
             participants=[], short_pseudonyms=[], participant_groups=[],
             force=False, resume=False, update=False):
        args = ['pull', '--output-directory', outputPath]
        if force:
            args.append('--force')
        if resume:
            args.append('--resume')
        if update:
            args.append('--update')

        for column in columns:
            args += ['--columns', column]
        for column_group in column_groups:
            args += ['--column-groups', column_group]
        for participant in participants:
            args += ['--participants', participant]
        for short_pseudonym in short_pseudonyms:
            args += ['--short-pseudonyms', short_pseudonym]
        for participant_group in participant_groups:
            args += ['--participant-groups', participant_group]

        self.run(args)


class PepOStream(io.RawIOBase):
    """Output stream for writing to PEP. Stdout and stderr will be stored in memory"""

    mode = 'w'

    def __init__(self, popen, cmd):
        self.popen = popen
        self.cmd = cmd
        self.pos = 0
        if popen.stdout is None and popen.stderr is None:
            return

        executor = concurrent.futures.ThreadPoolExecutor()
        if popen.stdout is not None:
            self.future_stdout = executor.submit(lambda s: s.read(),
                                                 popen.stdout)
        else:
            self.future_stdout = None
        if popen.stderr is not None:
            self.future_stderr = executor.submit(lambda s: s.read(),
                                                 popen.stderr)
        else:
            self.future_stderr = None

        executor.shutdown(wait=False)

    def writable(self) -> bool:
        return True

    def write(self, b):
        try:
            count = self.popen.stdin.write(b)
            self.pos += count
            return count
        except Exception as ex:
            stdout = None
            stderr = None
            try:
                (stdout, stderr) = self.wait()
            finally:
                raise PepError(self.cmd,
                    stdout = stdout, stderr = stderr) from ex

    def tell(self) -> int:
        return self.pos


    def close(self):
        try:
            self.popen.stdin.close()
            super().close()
        except Exception as ex:
            raise PepError(self.cmd) from ex

    def wait(self):
        self.close()
        stdout = self.future_stdout.result() \
            if self.future_stdout is not None \
            else None
        stderr = self.future_stderr.result() \
            if self.future_stderr is not None \
            else None
        returncode = self.popen.wait()
        if returncode != 0:
            raise PepError(self.cmd, returncode = returncode, stdout = stdout,
                stderr = stderr)
        return (stdout, stderr)
    