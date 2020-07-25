#!/system/bin/sh

log -p i -t gki "GKI APEX preinstall hook starting."

# The pre-installed APEX does not contain any OTA payload. Just skip installing anything.
if [[ ! -f /apex/com.android.gki/etc/ota/payload.bin ]]; then
  log -p i -t gki "No payload.bin found. Exiting."
  sleep 5s
  exit 0
fi

log -p i -t gki "Applying payload.";

# TODO may need to set LD_LIBRARY_PATH to lib64/
if ! /system/bin/logwrapper /apex/com.android.gki/bin/update_engine_stable_client \
      --payload /apex/com.android.gki/etc/ota/payload.bin \
      --headers "$(cat /apex/com.android.gki/etc/ota/payload_properties.txt)"; then
    log -p e -t gki "OTA failed"
    sleep 5s
    exit 1
fi

log -p i -t gki "OTA successful"
sleep 5s
exit 0
