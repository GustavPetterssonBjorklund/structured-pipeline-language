export class SPLValue<T = unknown> {
  readonly value: T;
  readonly splType: string;
}

export class Int64 extends SPLValue<bigint> {
  constructor(value: bigint | number);
  add(other: Int64 | bigint): Int64;
  mul(other: Int64 | bigint): Int64;
}

export class Float64 extends SPLValue<number> {
  constructor(value: number);
  add(other: Float64): Float64;
  mul(other: Float64): Float64;
}

export class Bool extends SPLValue<boolean> {
  constructor(value: boolean);
}

export class String extends SPLValue<string> {
  constructor(value: string);
}

export class Outputs {
  constructor(values: Record<string, SPLValue>);
  readonly values: Record<string, SPLValue>;
}
