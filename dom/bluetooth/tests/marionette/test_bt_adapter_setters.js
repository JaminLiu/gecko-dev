MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const EMULATOR_ADDRESS = "56:34:12:00:54:52";
const EMULATOR_NAME = "Full Android on Emulator";
const EMULATOR_CLASS = 0x58020c;

const FAKE_BT_NAME = "JaminDevice";
const FAKE_BT_ADDRESS = "56:34:12:00:54:60";
const FAKE_BT_DEVICE = "\"56:34:12:00:54:60,JaminDevice\"";


function setName(aAdapter, aName) {
  let deferred = Promise.defer();

  let request = aAdapter.setName(aName);
  request.addEventListener("success", function() {
    log("setBtName success: " + aName);
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("setBtName error: " + aName);
    deferred.reject();
  });

  return deferred.promise;
}

function setDiscoverable(aAdapter, aIsDiscoverable) {
  let deferred = Promise.defer();

  let request = aAdapter.setDiscoverable(aIsDiscoverable);
  request.addEventListener("success", function() {
    log("setDiscoverable success: " + aIsDiscoverable);
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("setDiscoverable error: " + aIsDiscoverable);
    deferred.reject();
  });

  return deferred.promise;
}

function setDiscoverableTimeout(aAdapter, aTimeout) {
  let deferred = Promise.defer();

  let request = aAdapter.setDiscoverableTimeout(aTimeout);
  request.addEventListener("success", function() {
    log("setDiscoverableTimeout success: " + aTimeout);
    deferred.resolve();
  });
  request.addEventListener("error", function() {
    log("setDiscoverableTimeout error: " + aTimeout);
    deferred.reject();
  });

  return deferred.promise;
}

startBluetoothTest(true, function testCaseMain(aAdapter) {
  //aAdapter.setDiscoverable(true);
  //aAdapter.setDiscoverableTimeout(180);
  //aAdapter.setName("Jamin ABC");

  setDiscoverable(aAdapter,true).then(function() {
    is(aAdapter.discoverable, true, "Set discoverable failed.");
  });

  setDiscoverableTimeout(aAdapter,180).then(function() {
    is(aAdapter.discovering, 180, "Set discovering failed.");
  });

  setName(aAdapter,"JaminSetBtName").then(function() {
    is(aAdapter.name, "JaminSetBtName", "Set name failed.");
  });

  // Send back the pairing confirmation when adapter tries to pair with a remote device.
  //aAdapter.setPairingConfirmation(deviceAddress, flag);

  // Send back the requested Passkey code when adapter tries to pair with a remote device.
  //aAdapter.setPasskey(deviceAddress, key); // passkey is 6-digit number

  // Send back the requested PIN code when adapter tries to pair with a remote device.
  //aAdapter.setPinCode(deviceAddress, code); // pinCode is a string, example: 0000
});