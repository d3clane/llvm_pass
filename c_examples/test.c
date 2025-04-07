int func(int val1, int val2) {
   while (val1 < 10) {
    ++val1;
   }

   while (val2 < 10) {
    ++val2;
   }

   return val1 + val2;
}

int main() {
    func(0, 5);
}