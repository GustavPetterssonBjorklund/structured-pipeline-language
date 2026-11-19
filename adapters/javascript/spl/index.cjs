class SPLValue {
  constructor(value, type) { this.value = value; this.splType = type; }
  _binary(other, operation, Type) {
    if (this.splType === "Int64" && typeof other === "bigint") return new Type(operation(this.value, other));
    if (!(other instanceof this.constructor)) throw new TypeError(`cannot combine ${this.splType} and ${other?.splType}`);
    return new Type(operation(this.value, other.value));
  }
}

class Int64 extends SPLValue {
  constructor(value) {
    if (typeof value === "number") {
      if (!Number.isInteger(value) || !Number.isSafeInteger(value)) throw new TypeError("Int64 number must be a safe integer");
      value = BigInt(value);
    }
    if (typeof value !== "bigint" || value < -(2n ** 63n) || value > 2n ** 63n - 1n) throw new TypeError("Int64 requires a signed 64-bit integer");
    super(value, "Int64");
  }
  add(other) { return this._binary(other, (a, b) => a + b, Int64); }
  mul(other) { return this._binary(other, (a, b) => a * b, Int64); }
}

class Float64 extends SPLValue {
  constructor(value) { if (typeof value !== "number" || !Number.isFinite(value)) throw new TypeError("Float64 requires a finite number"); super(value, "Float64"); }
  add(other) { return this._binary(other, (a, b) => a + b, Float64); }
  mul(other) { return this._binary(other, (a, b) => a * b, Float64); }
}

class Bool extends SPLValue {
  constructor(value) { if (typeof value !== "boolean") throw new TypeError("Bool requires a boolean"); super(value, "Bool"); }
}

class StringValue extends SPLValue {
  constructor(value) { if (typeof value !== "string") throw new TypeError("String requires a string"); super(value, "String"); }
}

class Outputs {
  constructor(values) {
    if (!values || typeof values !== "object") throw new TypeError("Outputs requires an object");
    this.values = values;
  }
}

const childProcess = require("child_process");

function rawInput(value) {
  if (!(value instanceof SPLValue)) throw new TypeError("pipeline inputs must be SPL values");
  return `${value.splType}:${value.value}`;
}

function runPipeline(artifact, inputs, engine = "build/spl") {
  const args = ["run", artifact];
  for (const [name, value] of Object.entries(inputs)) args.push("--input", `${name}=${rawInput(value)}`);
  let output;
  try {
    output = childProcess.execFileSync(engine, args, { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
  } catch (error) {
    throw new Error(String(error.stderr || error.stdout || error.message || "SPL pipeline failed").trim());
  }
  const outputs = {};
  for (const line of output.trim().split(/\r?\n/)) {
    if (!line) continue;
    const [name, type, raw] = line.split("\t");
    if (type === "Int64") outputs[name] = new Int64(BigInt(raw));
    else if (type === "Float64") outputs[name] = new Float64(Number(raw));
    else if (type === "Bool") outputs[name] = new Bool(raw === "true");
    else outputs[name] = new StringValue(raw);
  }
  return outputs;
}

function loadPipeline(artifact, engine = "build/spl") {
  return { artifact, run: inputs => runPipeline(artifact, inputs, engine) };
}

function buildPipeline(source, output, engine = "build/spl") {
  const args = ["build", source];
  if (output) args.push("--output", output);
  try {
    childProcess.execFileSync(engine, args, { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
  } catch (error) {
    throw new Error(String(error.stderr || error.stdout || error.message || "SPL build failed").trim());
  }
  const artifact = output || source.replace(/\.spl(?::[^/]*)?$/, ".splb");
  return loadPipeline(artifact, engine);
}

module.exports = { SPLValue, Int64, Float64, Bool, String: StringValue, Outputs, buildPipeline, loadPipeline, runPipeline };
