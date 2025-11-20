
echo -e "/time" | nc 127.0.0.1 9034
echo -e "/stats" | nc 127.0.0.1 9034
echo -e "/shutdown" | nc 127.0.0.1 9034
#./ndm_test & echo -e "sample\n" | nc 0.0.0.0 9034
