const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");
const spl = require(path.resolve(__dirname, "../../adapters/javascript/spl/index.cjs"));

function argument(name) {
  const index = process.argv.indexOf(name);
  if (index < 0 || !process.argv[index + 1]) throw new Error(`missing ${name}`);
  return process.argv[index + 1];
}

function loadTarget(target) {
  const separator = target.lastIndexOf("::");
  if (separator < 1 || separator === target.length - 2) throw new Error("JavaScript target must be file.js::export or file.ts::export");
  const source = path.resolve(target.slice(0, separator));
  const name = target.slice(separator + 2);
  if (source.endsWith(".ts")) {
    const output = fs.mkdtempSync(path.join(require("os").tmpdir(), "spl-ts-"));
    const packageRoot = path.resolve(__dirname, "../../adapters/javascript");
    const config = path.join(output, "tsconfig.json");
    fs.writeFileSync(config, JSON.stringify({
      compilerOptions: {
        outDir: output,
        rootDir: path.dirname(source),
        target: "ES2020",
        module: "commonjs",
        esModuleInterop: true,
        skipLibCheck: true,
        baseUrl: packageRoot,
        paths: { "@structured-pipeline-language/spl": ["spl"] },
      },
      files: [source],
    }));
    const compile = spawnSync("tsc", ["--project", config], { encoding: "utf8" });
    if (compile.status !== 0) throw new Error(`TypeScript compilation failed: ${compile.stderr || compile.stdout}`);
    const compiled = path.join(output, path.basename(source, ".ts") + ".js");
    return { module: require(compiled), name };
  }
  return { module: require(source), name };
}

function wrap(type, value) {
  if (type === "Int64" && typeof value === "string") value = BigInt(value);
  if (type === "Int64") return new spl.Int64(value);
  if (type === "Float64") return new spl.Float64(value);
  if (type === "Bool") return new spl.Bool(value);
  if (type === "String") return new spl.String(value);
  throw new Error(`unsupported SPL type: ${type}`);
}

function unwrap(value, expected, name) {
  if (!(value instanceof spl.SPLValue) || value.splType !== expected) throw new Error(`JavaScript output '${name}' must be spl.${expected}`);
  return { type: value.splType, value: expected === "Int64" ? value.value.toString() : value.value };
}

function main() {
  const request = JSON.parse(fs.readFileSync(argument("--request"), "utf8"));
  const target = loadTarget(request.target);
  const fn = target.module[target.name];
  if (typeof fn !== "function") throw new Error(`JavaScript module has no callable export '${target.name}'`);
  const inputs = {};
  for (const [name, entry] of Object.entries(request.inputs)) inputs[name] = wrap(entry.type, entry.value);
  const returned = fn(inputs);
  const values = returned instanceof spl.Outputs ? returned.values : (request.outputs && Object.keys(request.outputs).length === 1 ? { [Object.keys(request.outputs)[0]]: returned } : null);
  if (!values) throw new Error("multiple JavaScript outputs require new spl.Outputs({...})");
  const outputs = {};
  for (const [name, type] of Object.entries(request.outputs)) {
    if (!(name in values)) throw new Error(`JavaScript output is missing '${name}'`);
    outputs[name] = unwrap(values[name], type, name);
  }
  fs.writeFileSync(argument("--response"), JSON.stringify({ version: 1, outputs }));
}

try { main(); } catch (error) { console.error(error.stack || error); process.exitCode = 1; }
