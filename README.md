Uniksowy Serwer Gadu-Gadu
=========================

USG to proof-of-concept sprzed kilku lat. Mia� zako�czy� bezsensowne dyskusje na li�cie ekg-devel. Zosta� napisany w kilka godzin, ale dzia�a�. Przesy�a wiadomo�ci, kolejkuje, informuje o zmianach stanu itd. Nie mia� trybu tylko dla znajomych, nie obs�ugiwa� starszych klient�w (jak na tamte czasy), ignorowania, blokowania itd.

Je�li nie wiesz, jak to uruchomi�, jak tego u�ywa�, NIE PISZ DO MNIE! W najlepszym wypadku zostaniesz zignorowany. Ten kod to tylko i wy��cznie demonstracja. Nie jest przeznaczony dla zwyk�ych u�ytkownik�w, ani nawet dla administrator�w. Je�li nie rozumiesz C, najlepiej skasuj, daj sobie spok�j i nie zawracaj g�owy innym. Nawet je�li jeste� programist�, te� daj spok�j. Ten kod nie jest w �aden spos�b wspierany, ani utrzymywany.

Jak to dzia�a?
-------------

W katalogu queue s� zapisywane wiadomo�ci do u�ytkownik�w, kt�rzy nie byli dost�pni. Format znajdziesz w msgqueue.c. W katalogu passwd s� zapisywane has�a u�ytkownik�w. Jedna linia tekstu w pliku, kt�rego nazwa jest numerem. W katalogu reasons s� zapisane powody niedost�pno�ci niedost�pnych u�ytkownik�w. I tak dalej.

Czego trzeba wi�cej? Dok�adniej sprawdza� b��dy, obs�ugiwa� katalog publiczny, hub, wielow�tkowo�� itd. 
