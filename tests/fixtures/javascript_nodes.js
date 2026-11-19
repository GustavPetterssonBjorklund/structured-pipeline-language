const spl = require("../../adapters/javascript/spl/index.cjs");

exports.add = ({ a, b }) => a.add(b);
exports.multiply = ({ a, b }) => a.mul(b);
exports.pair = ({ value }) => new spl.Outputs({ doubled: value.mul(new spl.Int64(2)), label: new spl.String("ok") });
