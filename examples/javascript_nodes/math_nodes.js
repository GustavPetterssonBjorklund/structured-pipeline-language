const spl = require("../../adapters/javascript/spl/index.cjs");

exports.multiply = ({ a, b }) => a.mul(b);

// Keep the package import available to demonstrate the typed JS boundary.
exports.asInt64 = value => new spl.Int64(value);
