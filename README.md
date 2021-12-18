wowiconify
==========
wowiconify takes a source input image and number of WoW spell icons and then produces
and output image where every pixel from the source input image is approximated by a
WoW spell icon.

Sound complicated and confusing? Fear not!

Here's an image, which is supposed to be worth no more and no less than 1000 words.

### Input Image
![](test.png)

### Output Image
[![](test_preview_out.png)](test_out.png)

Click on the image above to view it in high resolution.

Getting Started
---------------
This is highly unoptimized and was thrown together in a couple of hours just for
funsies as an experiment.

Keep that in mind before you use any of it in a _production_ setting.

### Linux/macOS
```bash
$ cc -O3 -lm src/wowiconify.c -o build/wowiconify
$ build/wowiconify out.png test.png spells/*.png
```

### Windows
VS 2019 and later.

```bash
$ cl /O2 src/wowiconify.c /link /out:build/wowiconify.exe
$ build\wowiconify out.png test.png spells/*.png
```

Credits
-------
* [stb single file libraries][3] by [Sean Barret][4]
* [4300+ WoW Retail Icons][1] by [barrens.chat][2]

Contribute
----------
* Fork the project.
* Make your feature addition or bug fix.
* Do **not** bump the version number.
* Create a pull request. Bonus points for topic branches.

License
-------
Copyright (c) 2021, Mihail Szabolcs

**wowiconify** is provided **as-is** under the **MIT** license.
For more information see LICENSE.

[1]: https://www.warcrafttavern.com/community/art-resources/icon-pack-4300-wow-retail-icons-in-png/
[2]: https://barrens.chat/
[3]: https://github.com/nothings/stb
[4]: https://twitter.com/nothings
