#Autor
Michał Cieśnik

Projekt na laboratorium Sieci Komputerowe 2.

#Wymagania
g++ >= 5.0

#Kompilacja
```
make release
```
Wersja z opcją debugowania, wyświetlająca więcej komunikatów:
```
make debug
```

#Użytkowanie
Wyświetlenie możliwych opcji:
```
xmftp -h
```
Uruchomienie serwera bez dostępu do całego systemu plików:
```
mkdir ftproot
sudo xmftp -j ftproot
```

#Użyty kod
[GenericMakefile](https://github.com/mbcrawfo/GenericMakefile) - Michael Crawford
