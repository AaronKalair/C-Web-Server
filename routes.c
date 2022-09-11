typedef struct {
  char* (*routeFnPtr)(int);
  char *routename;
} route;

char* state(int a)
{
    return "{\"status\":1}";
}

const int routeCount = 1;
route stateR = { .routeFnPtr = &state, .routename = "/state.json"};
const route *routes[routeCount] = {
  &stateR
};
