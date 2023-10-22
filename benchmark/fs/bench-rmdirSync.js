'use strict';

const assert = require('assert');
const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  type: ['existing', 'non-existing'],
  n: [1e3],
});

function main({ n, type }) {
  switch (type) {
    case 'existing': {
      for (let i = 0; i < n; i++) {
        fs.mkdirSync(tmpdir.resolve(`rmsync-bench-dir-${i}`), {
          recursive: true,
        });
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        fs.rmSync(tmpdir.resolve(`rmsync-bench-dir-${i}`), {
          recursive: true,
          maxRetries: 3,
        });
      }
      bench.end(n);
      break;
    }
    case 'non-existing': {
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.rmSync(tmpdir.resolve(`.non-existent-folder-${i}`), {
            recursive: true,
            maxRetries: 3,
          });
        } catch (err) {
          assert.match(err.message, /ENOENT/);
        }
      }
      bench.end(n);
      break;
    }
    default:
      new Error('Invalid type');
  }
}
