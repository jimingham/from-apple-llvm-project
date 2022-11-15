struct Base_1 {
  virtual ~Base_1() = default;
  int base_1_arr[10] = { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109 };
};

struct Base_2 {
  virtual ~Base_2() = default;
  int base_2_arr[10] = { 200, 201, 202, 203, 204, 205, 206, 207, 208, 209 };
};

struct Derived : public Base_1, Base_2
{
  virtual ~Derived() = default;
  int derived_int = 1000;
};

int
main()
{
  Derived my_derived;
  Base_1 *base_1_ptr = (Base_1 *) &my_derived;
  Base_1 &base_1_ref = (Base_1 &) my_derived;

  Base_2 *base_2_ptr = (Base_2 *) &my_derived;
  Base_2 &base_2_ref = (Base_2 &) my_derived;

  // Set a breakpoint here
  return my_derived.derived_int + base_1_ptr->base_1_arr[0] + base_2_ptr->base_2_arr[0];
  
}

