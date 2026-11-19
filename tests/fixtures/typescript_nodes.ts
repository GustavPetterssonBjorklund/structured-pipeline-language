import type { Int64 } from "@structured-pipeline-language/spl";

export function add({ a, b }: { a: Int64; b: Int64 }): Int64 {
  return a.add(b);
}

export function increment({ value }: { value: Int64 }): Int64 {
  return value.add(1n);
}
