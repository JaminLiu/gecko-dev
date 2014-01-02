MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const EMULATOR_ADDRESS = "56:34:12:00:54:52";
const EMULATOR_NAME = "Full Android on Emulator";
const EMULATOR_CLASS = 0x58020c;

const FAKE_BT_NAME = "JaminDevice";
const FAKE_BT_ADDRESS = "56:34:12:00:54:60";
const FAKE_BT_DEVICE = "\"56:34:12:00:54:60,JaminDevice\"";

let deviceNum = 0;
let discoveryDeferred = Promise.defer();
let TheDevice;
let theAdapter;

function onDeviceFound(evt) {
  var device = evt.device;
  deviceNum = deviceNum +1;

TheDevice = device;

//var devices = [device];
theAdapter.devices = [device];

  // How to verify? Need QEMU support.
  log("3. Jamin display devices: " + device.name );
  log("4. Jamin display address: " + device.address );

  log("5. Jamin display class: " + device.class );
  log("6. Jamin display connected: " + device.connected );

  log("7. Jamin display icon: " + device.icon );
  log("8. Jamin display paired: " + device.paired );

  log("9. Jamin display services: " + device.services );
  log("10. Jamin display uuids: " + device.uuids );

  //log("5. Jamin display length: " + theAdapter.devices.length );
  //log("5. Jamin display length: " + devices.length );

  ok(true, "Device found");

  discoveryDeferred.resolve();
}

function addFakeDevice() {
  let deferred = Promise.defer();
  log("Add a fake device.");
  emulator.run("bt radd " + FAKE_BT_DEVICE, function(result) {
    if (result.length > 0) {
      log("addFakeDevice done");
        deferred.resolve();
    } else {
      log("Failed to add fake device.");
      deferred.reject();
    }
  });
  return deferred.promise;
}

function startDiscovery(aAdapter) {
  let deferred = Promise.defer();

  log("startDiscovery ~~");

  let request = aAdapter.startDiscovery();
  request.addEventListener("success", function() {
    log("startDiscovery success");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("startDiscovery error");
    deferred.reject();
  });

  return deferred.promise;
}

function stopDiscovery(aAdapter) {
  let deferred = Promise.defer();

  log("stopDiscovery ~~");

  let request = aAdapter.stopDiscovery();
  request.addEventListener("success", function() {
    log("stopDiscovery success");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("stopDiscovery error");
    deferred.reject();
  });

  return deferred.promise;
}

function pair(aAdapter, aDevice) {
  let deferred = Promise.defer();

  log("pair ~~");

  let request = aAdapter.pair(aDevice);
  request.addEventListener("success", function() {
    log("pair success");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("pair error");
    deferred.reject();
  });

  return deferred.promise;
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  theAdapter = aAdapter;
  aAdapter.ondevicefound = onDeviceFound;
  //aAdapter.startDiscovery();
  //aAdapter.stopDiscovery();

  addFakeDevice().then(
    function() { startDiscovery(aAdapter); },
    function() { ok(false, "Add fake device failed."); }
    );

  discoveryDeferred.promise.then(
    function() {
      //log("5. Jamin display length: " + aAdapter.devices.length );
      //log("6. Jamin display devices: " + aAdapter.devices[0].name );
      //log("7. Jamin display address: " + aAdapter.devices[0].address );

      pair(aAdapter, TheDevice).then(
        function() { log("5. Pair success"); },
        function() { log("5. Pair failed"); }
        );

      stopDiscovery(aAdapter).then(
        function() { ok(true, "6. Stop discovery success."); },
        function() { ok(false, "6. Stop discovery failed."); }
        );
      }
    );
});