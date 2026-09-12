# badterm

Une terminal minimale pour macOS et Linux, construite autour d’un PTY et d’une boucle d’événements native.

## Architecture

- `src/pty.c`: crée le processus enfant avec `forkpty`, lance le shell ou la commande fournie et applique les changements de taille.
- `src/terminal.c`: met le terminal parent en mode raw et le restaure à la sortie.
- `src/event_loop.c`: multiplexe `stdin` et le master PTY avec `kqueue` sur macOS et `epoll` sur Linux.
- `src/main.c`: orchestre le cycle de vie, le transfert bidirectionnel des octets et `SIGWINCH`.

## Compilation

```sh
make
./badterm
./badterm /bin/bash --noprofile --norc
```

Sur Linux, le lien avec `libutil` est ajouté automatiquement par le `Makefile`.