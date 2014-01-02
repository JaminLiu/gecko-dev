MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const EMULATOR_ADDRESS = "56:34:12:00:54:52";
const EMULATOR_NAME = "Full Android on Emulator";
const EMULATOR_CLASS = 0x58020c;

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Checking adapter attributes ...");

//  is(aAdapter.name, EMULATOR_NAME, "adapter.name");
//  is(aAdapter.class, EMULATOR_CLASS, "adapter.class");
//  is(aAdapter.address, EMULATOR_ADDRESS, "adapter.address");
//  is(aAdapter.discovering, false, "adapter.discovering");
//  is(aAdapter.discoverable, false, "adapter.discoverable");
//  is(aAdapter.discoverableTimeout, 120, "adapter.discoverableTimeout");

  let name;
  let addr;
  let discoverable;
  let discovering;
  let enable; // It's not a property of BT adapter.

  emulator.run("bt get name", function(result) {
    name = result[0];
    is(result[1], "OK", "Failed to execute QEMU command [bt get name].");
    //is(name, aAdapter.name, "adapter.name");
    log("1. Jamin BT get name: " + name);
  });

  emulator.run("bt get addr", function(result) {
    addr = result[0];
    is(result[1], "OK", "Failed to execute QEMU command [bt get addr].");
    is(addr, aAdapter.address, "adapter.address");
    log("2. Jamin BT get addr: " + addr);
  });

  emulator.run("bt get discoverable", function(result) {
    discoverable = result[0];
    is(result[1], "OK", "Failed to execute QEMU command [bt get discoverable].");
    is(discoverable, aAdapter.discoverable, "adapter.discoverable");
    log("3. Jamin BT get discoverable: " + discoverable);
  });

  emulator.run("bt get discovering", function(result) {
    discovering = result[0];
    is(result[1], "OK", "Failed to execute QEMU command [bt get discovering].");
    is(discovering, aAdapter.discovering, "adapter.discovering");
    log("4. Jamin BT get discovering: " + discovering);
  });

  emulator.run("bt get enable", function(result) {
    enable = result[0];
    is(result[1], "OK", "Failed to execute QEMU command [bt get enable].");
    ok(enable, "Failed to enable bluetooth.");
    log("5. Jamin BT get enable: " + enable);
  });

  log("adapter class: " + aAdapter.class);
  log("adapter devices: " + aAdapter.devices);
  log("adapter devices.length: " + aAdapter.devices.length);
  log("adapter discoverableTimeout: " + aAdapter.discoverableTimeout);
  log("adapter uuids: " + aAdapter.uuids);

// TODO
// aAdapter.address // done
// aAdapter.class
// aAdapter.devices
// aAdapter.discoverable // done
// aAdapter.discoverableTimeout
// aAdapter.discovering // done
// aAdapter.name // done
// aAdapter.uuids

  log("6. Jamin emulator OK");
  ok(true, "6. Jamin emulator OK");

// aAdapter.ondevicefound
// aAdapter.onpairedstatuschanged
});


//