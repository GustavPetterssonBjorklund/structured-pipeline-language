interface Int64Value {
  add(other: Int64Value): Int64Value;
}

export function add({ a, b }: { a: Int64Value; b: Int64Value }): Int64Value {
  return a.add(b);
}
