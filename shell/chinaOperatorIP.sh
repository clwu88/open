#!/bin/sh
###############################################################
#   Copyright (C) 2013 All rights reserved.
#   author   : josh.wu
#   date     : 2013-11-04
#   descripe : 导出电信、联通、移动这几个ISP的网段
#
###############################################################


OPERATOR_TMP=operatorip.txt

echo "Extract CN ipv4"

wget -O - http://ftp.apnic.net/apnic/stats/apnic/delegated-apnic-latest | \
awk -F'|' 'function netlen(x, pow) {
    if (x <= 1)
        return pow;
    pow --;
    return netlen(x/2, pow);
}
{
    if ($2 == "CN" && $3 == "ipv4") {
        ip = $4;
        num = $5;
        pow = 32;
        mask = netlen(num, pow);
        print ip"/"mask
    }
}' | awk -F'/' 'function log_process(lineNum) {
    if (lineNum - last_progress >= 10) {
        printf("=") > "/dev/stderr";
        last_progress += 10;
    }
}
BEGIN {
    # print "# inetnum    netname    descr    <irt>mnt-by    <inetnum>mnt-lower"
    object = "-";
    netname = "-";
    descr = "-";
    mnt_lower = "-";
    mnt_by = "-";
    last_progress = 0;
}
{
    cidr = $0;
    cmd = "/usr/bin/whois -h whois.apnic.net " cidr;
    while ((cmd | getline line) > 0) {
        if (object == "-") {
            if (line ~ /inetnum:/)
                object = "inetnum";
            else if (line ~ /irt:/)
                object = "irt";
        } else {
            if (match(line, /^([^[:space:]]+):[[:space:]]*([[:alnum:]].+)$/, m)) {
                tag = m[1];
                value = m[2];
                if (object == "inetnum") {
                    if (tag == "netname") {
                        netname = value;
                    } else if (tag == "descr" && descr == "-") {
                        descr = value;
                    } else if (tag == "mnt-lower") {
                        mnt_lower = value;
                    } else if (tag == "source") {
                        # the end of < inetnum > object
                        object = "-";
                    } else {
                        # nothing to do, we only care about 3 attribute:
                        #   netname,
                        #   descr (the first one)
                        #   mnt-lower
                    }
                } else if (object == "irt") {
                    # NOTE: <irt> is optional for a record
                    if (tag == "mnt-by") {
                        mnt_by = value;
                    } else if (tag == "source") {
                        # the end of < irt > object
                        object = "-";
                    } else {
                        # nothing to do, we only care about 1 attribute:
                        #   mnt-by
                    }
                }
            }
        }
    }
    close(cmd);
    printf("%s|%s|%s|%s|%s\n", cidr, netname, descr, mnt_by, mnt_lower);
    object = "-";
    netname = "-";
    descr = "-";
    mnt_lower = "-";
    mnt_by = "-";

    log_process(NR);
}' 1>> $OPERATOR_TMP

echo
echo "Finish"
