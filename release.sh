#! /bin/bash
git tag -l
echo "Choose a tag to build:"
read tag
git checkout ${tag} 2>/dev/null
if [ $? -ne 0 ];then
    echo "invalid tag"
    exit 1
fi
cd src && make clean all
cd ../lib
cp libalog.a libalog.a.${tag}
cd ..
tar -cf alog_${tag}.tar lib/libalog.a* bin/* include/alog.h include/logpub.h include/alogtypes.h
