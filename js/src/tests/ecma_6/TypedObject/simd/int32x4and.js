// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 and';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var b = int32x4(10, 20, 30, 40);
  var c = SIMD.int32x4.and(a, b);
  assertEq(c.x, 0);
  assertEq(c.y, 0);
  assertEq(c.z, 2);
  assertEq(c.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

