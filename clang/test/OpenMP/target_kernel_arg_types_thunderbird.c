// Thunderbird kernels record their parameter types in the device image.
//
// The Thunderbird device calls a kernel through libffi, which needs each
// parameter's type to place it in the right register. An Inspire device compile
// emits <kernel>_ctypes beside the kernel: one code per parameter, in order,
// read from the kernel's own signature. The leading parameter is dyn_ptr. Every
// other parameter is a pointer, POINTER (11), or a by-copy scalar widened to
// i64, INT64 (7) -- floating-point, complex and narrow integer values included.
// One kernel per capture kind below pins which of the two each kind becomes.
//
// No other target emits it, and the host module carries no type information.
//
// REQUIRES: riscv-registered-target, x86-registered-target

// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -emit-llvm-bc %s -o %t-host.bc
// RUN: %clang_cc1 -fopenmp -x c -triple riscv64-inspire-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -fopenmp-is-target-device \
// RUN:     -fopenmp-host-ir-file-path %t-host.bc -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=TBIRD %s

// The codes are read from the signature before optimization and the kernel is
// externally visible, so -O2 must give the same codes.
// RUN: %clang_cc1 -fopenmp -x c -triple riscv64-inspire-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -fopenmp-is-target-device \
// RUN:     -fopenmp-host-ir-file-path %t-host.bc -O2 -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=TBIRD %s

// C++: the implicit this, and a scalar captured through a reference.
// RUN: %clang_cc1 -fopenmp -x c++ -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -emit-llvm-bc %s -o %t-host-cxx.bc
// RUN: %clang_cc1 -fopenmp -x c++ -triple riscv64-inspire-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -fopenmp-is-target-device \
// RUN:     -fopenmp-host-ir-file-path %t-host-cxx.bc -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=CXX %s

// Another OpenMP device does not get it.
// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=x86_64-unknown-linux-gnu -emit-llvm-bc %s -o %t-x86host.bc
// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=x86_64-unknown-linux-gnu -fopenmp-is-target-device \
// RUN:     -fopenmp-host-ir-file-path %t-x86host.bc -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=OTHER %s
// OTHER-NOT: _ctypes

// Nor does the host compile for an Inspire target.
// RUN: %clang_cc1 -fopenmp -x c -triple x86_64-unknown-linux-gnu \
// RUN:     -fopenmp-targets=riscv64-inspire-linux-gnu -emit-llvm %s -o - \
// RUN:   | FileCheck --check-prefix=HOST %s
// HOST-NOT: ctypes

#ifndef __cplusplus

// scale: dyn_ptr, n, out, in, s.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_scale_l{{[0-9]+}}_ctypes = weak_odr protected constant [5 x i8] c"\0B\07\0B\0B\07"
// TBIRD-DAG: @llvm.compiler.used = {{.*}}@__omp_offloading_{{.*}}scale_l{{[0-9]+}}_ctypes
// TBIRD-DAG: define weak_odr protected void @__omp_offloading_{{.*}}scale_l{{[0-9]+}}(ptr {{.*}}%dyn_ptr, i64 {{.*}}%n, ptr {{.*}}%out, ptr {{.*}}%in, i64 {{.*}}%s)
void scale(const double *restrict in, double *restrict out, int n, double s) {
#pragma omp target teams distribute parallel for map(to : in[0 : n])           \
    map(from : out[0 : n])
  for (int i = 0; i < n; ++i)
    out[i] = in[i] * s + 1.0;
}

// A kernel that captures nothing still has dyn_ptr.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_none_l{{[0-9]+}}_ctypes = weak_odr protected constant [1 x i8] c"\0B"
void k_none(void) {
#pragma omp target
  {}
}

double sink[1];
struct S8 { int a, b; };
struct S24 { double a, b, c; };
enum E { E0, E1 };

// By-copy scalars of eight bytes or fewer are widened to i64: dyn_ptr, sink, v.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_char_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_short_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_int_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_long_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_bool_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_enum_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_float_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_double_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_cfloat_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_fp_double_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
void k_char(char v) {
#pragma omp target
  sink[0] = v;
}
void k_short(short v) {
#pragma omp target
  sink[0] = v;
}
void k_int(int v) {
#pragma omp target
  sink[0] = v;
}
void k_long(long v) {
#pragma omp target
  sink[0] = v;
}
void k_bool(_Bool v) {
#pragma omp target
  sink[0] = v;
}
void k_enum(enum E v) {
#pragma omp target
  sink[0] = v;
}
void k_float(float v) {
#pragma omp target
  sink[0] = v;
}
void k_double(double v) {
#pragma omp target
  sink[0] = v;
}
void k_cfloat(float _Complex v) {
#pragma omp target
  sink[0] = __real__ v;
}
void k_fp_double(double v) {
#pragma omp target firstprivate(v)
  sink[0] = v;
}

// Wider by-copy values, mapped scalars and pointers are passed as pointers:
// dyn_ptr, sink, v.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_cdouble_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_s8_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_s24_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_map_double_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_defmap_float_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_isdevptr_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_ptr_copy_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\0B"
void k_cdouble(double _Complex v) {
#pragma omp target
  sink[0] = __real__ v;
}
void k_s8(struct S8 v) {
#pragma omp target
  sink[0] = v.a;
}
void k_s24(struct S24 v) {
#pragma omp target
  sink[0] = v.a;
}
void k_map_double(double v) {
#pragma omp target map(to : v)
  sink[0] = v;
}
void k_defmap_float(float v) {
#pragma omp target defaultmap(tofrom : scalar)
  sink[0] = v;
}
void k_isdevptr(double *p) {
#pragma omp target is_device_ptr(p)
  sink[0] = p[0];
}
void k_ptr_copy(double *p) {
#pragma omp target firstprivate(p)
  sink[0] = (double)(long)p;
}

// A variable-length array: dyn_ptr, sink, the bound, a, n.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_vla_l{{[0-9]+}}_ctypes = weak_odr protected constant [5 x i8] c"\0B\0B\07\0B\07"
void k_vla(int n, double a[n]) {
#pragma omp target map(tofrom : a[0 : n])
  sink[0] = a[n - 1];
}

// More parameters than the eight integer argument registers: dyn_ptr, sink and
// twelve widened scalars.
// TBIRD-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}_k_many_l{{[0-9]+}}_ctypes = weak_odr protected constant [14 x i8] c"\0B\0B\07\07\07\07\07\07\07\07\07\07\07\07"
void k_many(double a, double b, double c, double d, double e, double f,
            double g, double h, double i, double j, float k, int l) {
#pragma omp target
  sink[0] = a + b + c + d + e + f + g + h + i + j + k + l;
}

#else

// CXX-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}__ZN1C3runEv_l{{[0-9]+}}_ctypes = weak_odr protected constant [2 x i8] c"\0B\0B"
// CXX-DAG: @__omp_offloading_{{[0-9a-f]+_[0-9a-f]+}}__ZN1C3refERd_l{{[0-9]+}}_ctypes = weak_odr protected constant [3 x i8] c"\0B\0B\07"
struct C {
  double s;
  double *p;
  int n;
  // dyn_ptr, this.
  void run() {
#pragma omp target map(tofrom : p[0 : n])
    p[0] *= s;
  }
  // dyn_ptr, this, r: a scalar referenced by a reference is captured by copy.
  void ref(double &r) {
#pragma omp target
    p[0] = r;
  }
};
void use(C &c, double &r) {
  c.run();
  c.ref(r);
}

#endif
