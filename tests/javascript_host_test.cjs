const assert = require("assert");
const spl = require("../adapters/javascript/spl/index.cjs");

const pipeline = spl.buildPipeline(
  "tests/fixtures/nested_cross_language.spl:Main",
  "/tmp/spl-nested-javascript-host.splb",
  "build/spl",
);
const result = pipeline.run({ x: new spl.Int64(3), y: new spl.Int64(4) });
assert.strictEqual(result.result.value, 15n);
