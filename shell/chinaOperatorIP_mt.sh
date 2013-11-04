#!/bin/sh
###############################################################
#   Copyright (C) 2013 All rights reserved.
#   author   : josh.wu
#   date     : 2013-11-04
#   descripe : 导出电信、联通、移动这几个ISP的网段
#              并行版本
#
###############################################################


OPERATOR_TMP=operatorip.txt

echo "Extract CN ipv4"

wget -O - http://ftp.apnic.net/apnic/stats/apnic/delegated-apnic-latest | \
awk -F'|' 'function netlen(x, pow) {
    if (x <= 1)
        return pow;
    return netlen(x/2, --pow);
}
{
    if ($2 == "CN" && $3 == "ipv4") {
        ip = $4;
        count = $5;
        pow = 32;
        mask = netlen(count, pow);
        print ip"/"mask
    }
}' | parallel -j+0 --pipe --block 128 --delay 2 -q awk -F'/' 'function log_process() {
    printf("=") > "/dev/stderr";
}
BEGIN {
    # print "# inetnum    netname    descr    <irt>mnt-by    <inetnum>mnt-lower"
    object = "-";
    netname = "-";
    descr = "-";
    mnt_lower = "-";
    mnt_by = "-";
    last_process = 0;
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
}
END {
    log_process();
}' 1>>$OPERATOR_TMP


rm cnc.txt
rm cmcc.txt
rm other.txt

cat $OPERATOR_TMP | awk -F'|' '{
mnt=$4;
if ($4 == "-")
    mnt=$5;
if (mnt ~ /MAINT-CERNET/) {
    # 教育网
    cernet ++;
} else if (mnt ~ /MAINT-CHINANET/) {
    # 电信
    chinanet ++;
} else if (mnt ~ /MAINT-CNCGROUP/) {
    # 网通
    cnc ++;
    print $1 >> "cnc.txt"
} else if (mnt ~ /MAINT-CN-CMCC/) {
    # 移动
    cmcc ++;
    print $1 >> "cmcc.txt"
} else if (mnt ~ /MAINT-CNNIC/) {
    # 中国互联网络信息中心
    cnnic ++;
} else
    other ++;
}
END {
    printf("%+8s %s\n", chinanet, "MAINT-CHINANET");
    printf("%+8s %s\n", cnc, "MAINT-CNCGROUP");
    printf("%+8s %s\n", cmcc, "MAINT-CN-CMCC");
    printf("%+8s %s\n", cnnic, "MAINT-CNNIC");
    printf("%+8s %s\n", other, "OTHER");
}'

# 联通路由
cat cnc.txt | ./cidrmerge | ./cidrtonetmask.pl |awk '{print "route add " $1 " mask " $2 " 192.168.201.1";}' > cnc.bat
# 移动路由
cat cmcc.txt | ./cidrmerge | ./cidrtonetmask.pl |awk '{print "route add " $1 " mask " $2 " 192.168.100.1";}' > cmcc.bat
# 非联通、非移动的网段全都路由到电信
cat <<EOF > other.bat
route add 0.0.0.0 mask 128.0.0.0 192.168.200.1
route add 128.0.0.0 mask 128.0.0.0 192.168.200.1
EOF

echo
echo "Finish"
