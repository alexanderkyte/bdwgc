typedef union {
  int left;
  double* right;
} Either;

int main(void){
    Either alter;
    alter.left = 10;

    double a = 10;
    Either alter2;
    alter2.right = &a;

    return (double)alter.left + *alter.right;
}
