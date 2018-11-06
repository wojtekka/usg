Uniksowy Serwer Gadu-Gadu
=========================

USG to proof-of-concept sprzed kilku lat. Mia³ zakoñczyæ bezsensowne dyskusje na li¶cie ekg-devel. Zosta³ napisany w kilka godzin, ale dzia³a³. Przesy³a wiadomo¶ci, kolejkuje, informuje o zmianach stanu itd. Nie mia³ trybu tylko dla znajomych, nie obs³ugiwa³ starszych klientów (jak na tamte czasy), ignorowania, blokowania itd.

Je¶li nie wiesz, jak to uruchomiæ, jak tego u¿ywaæ, NIE PISZ DO MNIE! W najlepszym wypadku zostaniesz zignorowany. Ten kod to tylko i wy³±cznie demonstracja. Nie jest przeznaczony dla zwyk³ych u¿ytkowników, ani nawet dla administratorów. Je¶li nie rozumiesz C, najlepiej skasuj, daj sobie spokój i nie zawracaj g³owy innym. Nawet je¶li jeste¶ programist±, te¿ daj spokój. Ten kod nie jest w ¿aden sposób wspierany, ani utrzymywany.

Jak to dzia³a?
-------------

W katalogu queue s± zapisywane wiadomo¶ci do u¿ytkowników, którzy nie byli dostêpni. Format znajdziesz w msgqueue.c. W katalogu passwd s± zapisywane has³a u¿ytkowników. Jedna linia tekstu w pliku, którego nazwa jest numerem. W katalogu reasons s± zapisane powody niedostêpno¶ci niedostêpnych u¿ytkowników. I tak dalej.

Czego trzeba wiêcej? Dok³adniej sprawdzaæ b³êdy, obs³ugiwaæ katalog publiczny, hub, wielow±tkowo¶æ itd. 
