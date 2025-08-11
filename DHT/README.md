## Основные компоненты

### 1. `DHTNode` — основной класс

Реализует сервис DHT и интегрируется с Raft:

```cpp
class DHTNode final : public DHTService::Service {
    // Реализация методов Put, Get, Delete
};
```

---

### 2. Механизм репликации

Все операции (`PUT`, `DELETE`) преобразуются в команды и отправляются через **Raft**:

```proto
message Command {
  enum Type {
    PUT = 0;
    DELETE = 1;
  }
  Type type = 1;
  bytes key = 2;
  bytes value = 3;
}
```

---

### 3. Хранение данных

Данные хранятся в локальном `std::map` после применения через Raft:

```cpp
std::map<std::string, std::string> storage_;
```

---

### 4. API

```proto
service DHTService {
  rpc Put(PutRequest) returns (PutResponse);
  rpc Get(GetRequest) returns (GetResponse);
  rpc Delete(DeleteRequest) returns (DeleteResponse);
}
```

#### Методы:

- **Put** — сохранение значения по ключу
  ```proto
  rpc Put(PutRequest) returns (PutResponse);
  message PutRequest {
    bytes key = 1;
    bytes value = 2;
  }
  ```

- **Get** — получение значения по ключу
  ```proto
  rpc Get(GetRequest) returns (GetResponse);
  message GetRequest {
    bytes key = 1;
  }
  ```

- **Delete** — удаление значения по ключу
  ```proto
  rpc Delete(DeleteRequest) returns (DeleteResponse);
  message DeleteRequest {
    bytes key = 1;
  }
  ```
