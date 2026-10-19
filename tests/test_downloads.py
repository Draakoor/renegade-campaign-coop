from functools import partial
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer
from pathlib import Path
import tempfile
import threading
import unittest
from test_install import runtime


class DownloadsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.repo = root / 'ttfs'
        (self.repo / 'files').mkdir(parents=True)
        (self.repo / 'files' / 'test.tpi').write_bytes(b'package data')
        (root / 'server.ini').write_text('private configuration')
        self.http = ThreadingHTTPServer(('127.0.0.1', 0), partial(runtime.PackageHandler, directory=str(self.repo)))
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.close)

    def close(self):
        self.http.shutdown()
        self.http.server_close()
        self.thread.join()

    def request(self, method, path):
        conn = HTTPConnection('127.0.0.1', self.http.server_port, timeout=3)
        conn.request(method, path)
        response = conn.getresponse()
        result = response.status, response.read()
        conn.close()
        return result

    def test_download_and_head(self):
        self.assertEqual(self.request('GET', '/files/test.tpi'), (200, b'package data'))
        self.assertEqual(self.request('HEAD', '/files/test.tpi'), (200, b''))

    def test_package_root_uses_installed_paths_and_rejects_escape(self):
        server = Path(self.temp.name) / 'server'
        (server / 'data').mkdir(parents=True)
        config = server / 'data' / 'paths.ini'
        config.write_text('[paths]\nFileBase=ExistingCampaign\nFileFDS=FDS\n')
        self.assertEqual(runtime.package_directory(server), server / 'ExistingCampaign' / 'FDS' / 'ttfs')
        config.write_text('[paths]\nFileBase=../private\nFileFDS=FDS\n')
        with self.assertRaises(RuntimeError):
            runtime.package_directory(server)

    def test_private_paths_and_directory_listing_are_not_served(self):
        for path in ('/', '/files/', '/server.ini', '/files/../../server.ini', '/files/%2e%2e/%2e%2e/server.ini'):
            self.assertEqual(self.request('GET', path)[0], 404, path)


if __name__ == '__main__':
    unittest.main()
