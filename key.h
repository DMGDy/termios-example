typedef enum {
  NOOP,
  UP,
  DOWN,
  LEFT,
  RIGHT,
} KEY;

typedef struct {
  KEY* keys;
  size_t cap;
  size_t head;
} keys_t;




