#!/bin/sh

BINPATH="/usr/bin"

if [ -e /nvram/webpa_cfg.json ]; then
	echo "webpa_cfg.json exists in nvram"
 else
	echo "webpa_cfg.json not found in nvram"
	cp /etc/webpa_cfg.json /nvram/webpa_cfg.json
	echo "webpa_cfg.json file does not exist. Hence copying the factory default file.."
fi
        
WEBPAVER=`cat /nvram/webpa_cfg.json | grep "file-version" | awk '{print $2}' | sed 's|[\"\",]||g'`
echo "WEBPAVER is $WEBPAVER"
if [ "$WEBPAVER" = "" ];then
	cp /etc/webpa_cfg.json /nvram/webpa_cfg.json
	echo "Copying factory default file as webpa file-version does not exist in current cfg file.."
fi
	
ENABLEWEBPA=`cat /nvram/webpa_cfg.json | grep EnablePa | awk '{print $2}' | sed 's|[\"\",]||g'`
echo "ENABLEWEBPA is $ENABLEWEBPA"

if [ -e /nvram/disablewebpa ]; then
	echo "***Disabling webpa*****"
elif [ "$ENABLEWEBPA" = "true" ];then
	echo "ENABLEWEBPA is true..Intializing WebPA.."
	if [ "x"$Subsys = "x" ];then
		$BINPATH/webpa
	else
		echo "./webpa -subsys $Subsys"
		$BINPATH/webpa -subsys $Subsys
	fi
else
	echo "EnablePa parameter is set to false. Hence not initializng WebPA.."
fi
