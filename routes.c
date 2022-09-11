typedef struct {
  char* (*routeFnPtr)(int);
  char *routename;
} route;

// TO ADD A ROUTE:
// 1. define a function that returns a char* here (that is the json content)
// 2. add a route declaration at the bottom
//    like: `route stateR = { .routeFnPtr = &state, .routename = "/state.json"};`

char* state(int a)
{
    return "{\"status\":1}";
}

const int routeCount = 1;
route stateR = { .routeFnPtr = &state, .routename = "/state.json"};
const route *routes[routeCount] = {
  &stateR
};
