#!/bin/bash

if [[ -z "$IDF_PATH" ]]
then
    source export.sh --skip-source-check
fi

qtcreator "DmxFirmware" 2>&1 >/dev/null &
