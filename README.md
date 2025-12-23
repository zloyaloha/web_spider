# Паучок

### Robot

Чтобы запустить робота нужно:

```bash
cd robot
go mod tidy
go run ./cmd/main.go
```

### Searcher

Чтобы запустить поисковик

```bash
mkdir build
cd build
cmake ..
make
./doc_searcher -i
```