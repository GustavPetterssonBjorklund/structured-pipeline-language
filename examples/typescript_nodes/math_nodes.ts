import type { Int64 } from "@structured-pipeline-language/spl";

export function increment({ value }: { value: Int64 }): Int64 {
  return value.add(1n);
}
