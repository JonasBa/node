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
        fs.writeFileSync(tmpdir.resolve(`rimrafunlinksync-bench-dir-${i}`), '');
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.rmSync(tmpdir.resolve(`rimrafunlinksync-bench-dir-${i}`), {
            recursive: true, // required to enter rimraf path
            maxRetries: 3,
          });
        } catch (err) {
          throw err;
        }
      }
      bench.end(n);
      break;
    }
    case 'non-existing': {
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          fs.rmSync(tmpdir.resolve(`.non-existent-folder-${i}`), {
            recursive: true, // required to enter rimraf path
            maxRetries: 3,
          });
        } catch (err) {
          assert.ok(err);
        }
      }
      bench.end(n);
      break;
    }
    default:
      new Error('Invalid type');
  }
}
