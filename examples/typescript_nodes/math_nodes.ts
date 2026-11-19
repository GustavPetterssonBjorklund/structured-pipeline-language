import type { Int64 } from "../../adapters/javascript/spl/index";

export function increment({ value }: { value: Int64 }): Int64 {
  return value.add(1n);
}
