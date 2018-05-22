# parodus2mockTr181

Simple interface provide a mock TR-181 object model

[![Build Status](https://travis-ci.org/Comcast/parodus2mockTr181.svg?branch=master)](https://travis-ci.org/Comcast/parodus2mockTr181)
[![codecov.io](http://codecov.io/github/Comcast/parodus2mockTr181/coverage.svg?branch=master)](http://codecov.io/github/Comcast/parodus2mockTr181?branch=master)
[![Coverity](https://img.shields.io/coverity/scan/invalid.svg)]("https://scan.coverity.com/projects/comcast-parodus2mockTr181)
[![Apache V2 License](http://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/Comcast/parodus2mockTr181/blob/master/LICENSE.txt)

# Building and Testing Instructions

```
mkdir build
cd build
cmake ..
make
make test
```

# To Test the parodus2mockTr181:

```
1. Run parodus as stand alone. e.g.

     ~$ sudo ./parodus --hw-model=TGXXX --hw-serial-number=E8GBUEXXXXXXXXX --hw-manufacturer=ARRIS --hw-mac=aabbcc123456 --hw-last-reboot-reason=unknown --fw-name=TG1682_DEV_master_2017xxxxxxxxsdy --boot-time=1494590301 --webpa-ping-time=180 --webpa-interface-used=<eth interface> --webpa-url=<URL> --webpa-backoff-max=9 --parodus-local-url=tcp://127.0.0.1:6666 --partner-id=xxxxxx --ssl-cert-path=/etc/ssl/certs/ca-certificates.crt --webpa-token=/usr/ccsp/parodus/parodus_token.sh --force-ipv4 --secure-flag=https --port=8080

2. Run parodus2mockTr181 as stand alone. e.g.

    ~$ ./mock_tr181

3. Issue curl commands to test. e.g.

    ~$ curl -H "Authorization: Bearer $WEBPA_SAT" https://xxx.yyy.zzz:ppp/api/v2/device/mac:aabbcc123456/config?names=Device.Hosts.Host.1.Name,Device.Hosts.Host.1.MAC,Device.Hosts.Host.2.Name,Device.Hosts.Host.2.MAC

```
