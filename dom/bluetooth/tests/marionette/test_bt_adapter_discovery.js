MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const EMULATOR_ADDRESS = "56:34:12:00:54:52";
const EMULATOR_NAME = "Full Android on Emulator";
const EMULATOR_CLASS = 0x58020c;

const FAKE_BT_ADDRESS = "56:34:12:00:54:60";
const FAKE_BT_NAME = "JaminDevice";

let deviceNum = 0;
let discoveryDeferred = Promise.defer();
let TheDevice;
let theAdapter;

// 1. Add Remote Device
function addRemoteDevice() {
  log("1. Add Remote Device    \t,");
  let deferred = Promise.defer();

  emulator.run("bt add-remote " + FAKE_BT_ADDRESS, function(result) {
    if (result.length > 0) {
      log("   1. Done    \t,");
      deferred.resolve();
    } else {
      log("   1. Fail    \t,");
      deferred.reject();
    }
  });
  return deferred.promise;
}

// 2. Set Remote Device
function setRemoteDeviceName() {
  log("2.Set Remote device.    \t,");
  let deferred = Promise.defer();

  emulator.run("bt set " + FAKE_BT_ADDRESS + " name " + FAKE_BT_NAME, function(result) {
    if (result.length > 0) {
      log("   2. Done    \t,");
        deferred.resolve();
    } else {
      log("   2. Fail?    \t,");
      deferred.resolve();
      //deferred.reject();
    }
  });
  return deferred.promise;
}

// 3. Start Discovery
function startDiscovery(aAdapter) {
  log("3. Start Discovery    \t,");
  let deferred = Promise.defer();

  let request = aAdapter.startDiscovery();
  request.addEventListener("success", function() {
    log("   3. Done    \t,");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("   3. Fail    \t,");
    deferred.reject();
  });

  return deferred.promise;
}

function onDeviceFound(evt) {
  log("  4. onDeviceFound    \t,");
  var device = evt.device;
  deviceNum = deviceNum +1;

  TheDevice = device;

  //var devices = [device];
  theAdapter.devices = [device];

  // How to verify? Need QEMU support.
  log("   4. Jamin display devices: " + device.name + "    \t,");
  log("   4. Jamin display address: " + device.address + "    \t,");

  //log("4. Jamin display class: " + device.class );
  //log("4. Jamin display connected: " + device.connected );
  //log("4. Jamin display icon: " + device.icon );
  //log("4. Jamin display paired: " + device.paired );
  //log("4. Jamin display services: " + device.services );
  //log("4. Jamin display uuids: " + device.uuids );
  //log("4. Jamin display length: " + theAdapter.devices.length );
  //log("4. Jamin display length: " + devices.length );

  ok(true, "Device found");
  discoveryDeferred.resolve();
}

// 5. Pairing
function pair(aAdapter, aDevice) {
  log("5. Pairing    \t,");
  let deferred = Promise.defer();

  let request = aAdapter.pair(aDevice);
  request.addEventListener("success", function() {
    log("   5. Done    \t,");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("   5. Fail    \t,");
    deferred.reject();
  });

  return deferred.promise;
}

// 6. Stop discovery
function stopDiscovery(aAdapter) {
  log("6. Stop discovery    \t,");
  let deferred = Promise.defer();

  let request = aAdapter.stopDiscovery();
  request.addEventListener("success", function() {
    log("   6. Done    \t,");
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("   6. Fail    \t,");
    deferred.reject();
  });

  return deferred.promise;
}

// 7. Remove device
function removeRemoteDevice() {
  log("7. Remove device    \t,");
  let deferred = Promise.defer();

  //emulator.run("bt remove-remote " + FAKE_BT_ADDRESS, function(result) {
    emulator.run("bt remove-remote all", function(result) {
    if (result.length >= 0) {
      log("   7. Done    \t,");
        deferred.resolve();
    } else {
      log("   7. Fail?    \t,");
      deferred.resolve();
      //deferred.reject();
    }
  });
  return deferred.promise;
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  // 0. Enable BT, Add a BT Adapter
  // 1. Add Device
  // 2. Set Name
  // 3. Discovery
  // 4. Device found
  // 5. Pair
  // 6. Stop discover
  // 7. Remove device

  theAdapter = aAdapter;
  aAdapter.ondevicefound = onDeviceFound;

  return removeRemoteDevice()
    .then(function() { return addRemoteDevice(); })
    .then(function() { return setRemoteDeviceName(); })
    .then(function() { return startDiscovery(aAdapter); })
    .then(function() { return discoveryDeferred.promise; })
  //.then( Pairing )
    .then(function() { return stopDiscovery(aAdapter); } )
    .then(function() { return removeRemoteDevice(); });
});