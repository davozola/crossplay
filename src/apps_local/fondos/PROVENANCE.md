# Where the wallpapers come from

Every picture in `fondos.pack` is a work from the Art Institute of Chicago's open-access
collection, which the museum marks as public domain and offers for unrestricted use
(https://www.artic.edu/open-access/open-access-images). They were found with the museum's
public API by `tools_local/wallpapers/aic_fondos.py`, and cropped to the panel and dithered
to black and white by `tools_local/wallpapers/make_pack.py` with `--dither stucki`
(error diffusion with the Stucki kernel and a serpentine scan, from `tone.py`). The list below is in pack order;
each number is the work's id at `https://www.artic.edu/artworks/<id>`.

| # | Work | Artist | Date | AIC id |
| --- | --- | --- | --- | --- |
| 1 | Kanaya, from the series "Fifty-three Stations of the Tokaido (Tokaido gojusan tsugi)" | Katsushika Hokusai | c. 1806 | 2941 |
| 2 | Yoro Waterfall in Mino Province (Mino no kuni Yoro no taki), from the series Tour of the Waterfalls in Various Provinces (Shokoku Takimeguri) | Katsushika Hokusai | c. 1833 | 13255 |
| 3 | Ono Falls on the Kisokaidō Road (Kisokaidō Ono no bakufu), from the series Tour of the Waterfalls in Various Provinces (Shokoku taki meguri) | Katsushika Hokusai | c. 1833 | 25183 |
| 4 | A Peasant Crossing a Bridge, from the series A True Mirror of Chinese and Japanese Poems | Katsushika Hokusai | late 1830s | 57271 |
| 5 | Mimasaka Province: Yamabushi Valley (Mimasaka, Yamabushidani), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1853 | 13142 |
| 6 | Omi Province: Lake Biwa and Ishiyama Temple (Omi, Biwako Ishiyamadera), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1853 | 13152 |
| 7 | Bingo Province: Kannon Temple at Abuto (Bingo, Abuto, Kannondo), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1853 | 13154 |
| 8 | Hilltop View from Yushima Tenjin Shrine (Yushima Tenjin sakaue tenbo), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1856 | 13171 |
| 9 | Precints of the Akiba Shrine, Ukeji (Ukeji Akiba no keidai), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 13188 |
| 10 | Asakusa Rice Fields and Torinomachi Festival (Asakusa tanbo Torinomachi mode), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 13192 |
| 11 | Bamboo Yards and Kyo Bridge (Kyobashi Takegashi), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 13198 |
| 12 | Oumayagashi, from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 18366 |
| 13 | Autumn Moon over Ishiyama Temple (Ishiyama shugetsu), from the series "Eight Views of Omi (Omi hakkei)" | Utagawa Hiroshige | 1857 | 18972 |
| 14 | Plum Garden at Kameido (Kameido Umeyashiki), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 26577 |
| 15 | Drum Bridge and Yuhi Hill at Meguro (Meguro Taikobashi Yuhi-no-oka), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 26615 |
| 16 | Kinryuzan Temple at Asakusa (Asakusa Kinryuzan), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1856 | 34096 |
| 17 | Ryogoku Bridge and the Great Riverbank (Ryogokubashi Okawabata), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1856 | 34277 |
| 18 | Kai Province: Monkey Bridge (Kai, Saruhashi), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1853 | 130542 |
| 19 | Hida Province: Basket Ferry (Hida, Kagowatashi), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1853 | 130558 |
| 20 | Awa Province: Naruto Whirlpools (Awa, Naruto no fuha), from the series "Famous Places in the Sixty-odd Provinces (Rokujuyoshu meisho zue)" | Utagawa Hiroshige | 1855 | 130577 |
| 21 | Surugacho, from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1856 | 196735 |
| 22 | Night Rain at the Paulownia Grove at Akasaka (Akasaka Kiribatake uchu yukei), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige II (Shigenobu) | 1859 | 13229 |
| 23 | Tsuten-kyo Bridge, Tofuku Temple, Kyoto (Kyoto Tofukuji Tsutenkyo bashi) from the series “One Hundred Famous Views in the Various Provinces (Shokoku meisho hyakkei)” | Utagawa Hiroshige II (Shigenobu) | 1859 | 196732 |
| 24 | Kintai Bridge at Iwakuni, Suo (Boshu) Province from the series “One Hundred Famous Views of the Various Provinces” | Utagawa Hiroshige II (Shigenobu) | 1859 | 196747 |
| 25 | Actual View of Mount Tate, Etchu Province (Etchu Tateyama shinkei) "One Hundred Famous Views in the Various Provinces (Shokoku meisho hyakkei)" | Utagawa Hiroshige II (Shigenobu) | 1859 | 196844 |
| 26 | Image of a Whale Hunt at Goto, Hizen Province (Hizen Goto geiryo no zu), from the series “One Hundred Famous Views in the Various Provinces (Shokoku meisho hyakkei)” | Utagawa Hiroshige II (Shigenobu) | 1859 | 196846 |
| 27 | Cranes on snow-covered pine | Katsushika Hokusai | c. 1834 | 21720 |
| 28 | Great American Shrike | Robert Havell | 1834 | 18876 |
| 29 | Florida Jay | Robert Havell | 1830 | 18878 |
| 30 | Black Backed Gull | Robert Havell | 1826/39 | 183040 |
| 31 | Sparrows and Camellia in Snow | Utagawa Hiroshige | c. 1831-33 | 11039 |
| 32 | Japanese white-eye on flowering branch | Utagawa Hiroshige | 1830s–1840s | 19018 |
| 33 | Fukagawa Susaki and Jumantsubo (Fukagawa Susaki Jumantsubo), from the series "One Hundred Famous Views of Edo (Meisho Edo hyakkei)" | Utagawa Hiroshige | 1857 | 26605 |
| 34 | The First Knot | Albrecht Dürer | c. 1507 | 2273 |
| 35 | The Fifth Knot | Albrecht Dürer | c. 1507 | 2287 |
| 36 | St. Christopher Facing to the Right | Albrecht Dürer | 1521 | 4107 |
| 37 | Erasmus of Rotterdam | Albrecht Dürer | 1526 | 4118 |
| 38 | The Annunciation, from The Life of the Virgin | Albrecht Dürer | c. 1503, published 1511 | 4152 |
| 39 | Knight, Death, and the Devil | Albrecht Dürer | 1513 | 11100 |
| 40 | Nativity | Albrecht Dürer | 1504 | 14448 |
| 41 | Melencolia I | Albrecht Dürer | 1514 | 28260 |
| 42 | The Prodigal Son Amid the Swine | Albrecht Dürer | c. 1496 | 28266 |
| 43 | Saint Jerome in his Cell | Albrecht Dürer | 1511 | 106551 |
| 44 | The Sleep of Reason Produces Monsters, plate 43 from Los Caprichos | Francisco José de Goya y Lucientes | 1797/99 | 44743 |
| 45 | The Smoking Fire, plate 6 from Imaginary Prisons | Giovanni Battista Piranesi | 1761 | 7356 |
| 46 | The Drawbridge, plate 7 from Imaginary Prisons | Giovanni Battista Piranesi | 1761 | 7358 |
| 47 | The Column of Marcus Aurelius, from Views of Rome | Giovanni Battista Piranesi | 1750/59 | 64511 |
| 48 | The Round Tower, plate 3 from the second edition of Carceri d'invenzione (Imaginary Prisons) | Giovanni Battista Piranesi | 1750, reworked 1761 | 74624 |
| 49 | Beggar without Hat or Shoes, plate twelve from The Beggars | Jacques Callot | c. 1622 | 18054 |
| 50 | Ornament with Owl Mocked by Day Birds | Martin Schongauer | c. 1474 | 4016 |
| 51 | The Temptation of Saint Anthony | Martin Schongauer | c. 1469/73 | 84751 |
| 52 | Self-Portrait Leaning on a Stone Sill | Rembrandt van Rijn | 1639 | 79461 |
| 53 | Self-Portrait Etching at a Window | Rembrandt van Rijn | 1648 | 181616 |
| 54 | The Bishop's Tomb, from Vedute | Canaletto | 1735/44 | 159749 |
| 55 | The House with the Inscription and The House with the Peristyle, from Vedute | Canaletto | 1735/44 | 159768 |
| 56 | Entrance to the French Capuchin Monastery in Athens | Charles Meryon | 1854 | 28734 |
| 57 | Le Petit Pont, Paris | Charles Meryon | 1850 | 47194 |
| 58 | Chantrey Street, Paris | Charles Meryon | 1862 | 47238 |
| 59 | The Clock Tower, Paris | Charles Meryon | 1852 | 63891 |
| 60 | The Four Goats | Claude Lorrain | 1630/33 | 22630 |
| 61 | Small Vertical Landscape | Félix Hilaire Buhot | n.d. | 41604 |
| 62 | Trajan's Column, from Views of Rome | Giovanni Battista Piranesi | 1750/59 | 48108 |
| 63 | Another view of the Temple of the Sibyl in Tivoli, from Views of Rome | Giovanni Battista Piranesi | 1761 | 48164 |
| 64 | The Balcony | James McNeill Whistler | 1879/80 | 7741 |
| 65 | The Skylark | Samuel Palmer | n.d. | 22753 |
| 66 | Sunset | Samuel Palmer | c. 1850 | 47714 |
