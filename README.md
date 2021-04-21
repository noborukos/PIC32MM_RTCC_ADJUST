# PIC32MM_RTCC_ADJUST
PIC32MMのLPRCのばらつきが大きいので<br>
発振周波数を求めてからRTCCのクロック源にする案<br>
<br>
つまり時計として使うなら<br>
最初からSOSCに高精度なクロックを用意した方が良いのです<br>
