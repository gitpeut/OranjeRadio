# OranjeRadio

web radio with VS1053 and gesture control using PAJ7620
<img src="images/20201113_152249.jpg" /> 
<ul>
<li>
For the VS1053, this library is used :
https://github.com/baldram/ESP_VS1053_Library.
A modification has been applied to the header: both the read_register and the write register function have been changed from protected to public, to allow for applying the VS1053 patches.
</li>
<li>For the PAJ7620, this library is used:
https://github.com/Seeed-Studio/Gesture_PAJ7620 (also available via the Arduino library manager)
</li>
</ul>
For hardware used see:
<a href="https://oshwlab.com/peut/webradio">webradio</a>
