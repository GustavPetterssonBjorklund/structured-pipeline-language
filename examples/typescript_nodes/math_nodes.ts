interface Int64Value {
  add(other: bigint): Int64Value;
}

export function increment({ value }: { value: Int64Value }): Int64Value {
  return value.add(1n);
}
