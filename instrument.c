#include "Python.h"
#include "opcode.h"

static void merge_tuple(PyObject *attr1, PyObject *attr2, PyObject *target) {
  // Tuple contents of the first object are placed *AFTER* the second object.
  const long attr1_size = PyTuple_Size(attr1);  
  const long attr2_size = PyTuple_Size(attr2);	 
  target = PyTuple_New(attr1_size + attr2_size); 
  int i = 0;
  PyObject *item;
  for ( ; i < attr2_size ; ++i ) {
    item = PyTuple_GetItem(attr2, i);				
    Py_INCREF(item);	
    PyTuple_SetItem(target, i, item);
  }		
  for (i = 0 ; i < attr1_size ; ++i ) {
    item = PyTuple_GetItem(attr1, i);
    Py_INCREF(item);
    PyTuple_SetItem(target, i + attr2_size, item);
  } 
}

static long merge_bytecodes(PyCodeObject *inject, PyCodeObject *fn, char *bytecodes) {

  char *inject_bytecodes;
  char *fn_bytecodes;
  Py_ssize_t inject_bytecodes_len = 0;
  Py_ssize_t fn_bytecodes_len = 0;
  // Remove the final return from inject's bytecodes
  long newcode_size = inject_bytecodes_len + fn_bytecodes_len - 1;

  PyString_AsStringAndSize(inject->co_code, (char **)&inject_bytecodes, &inject_bytecodes_len);
  PyString_AsStringAndSize(fn->co_code, (char **)&fn_bytecodes, &fn_bytecodes_len);
  
  char *newcode = malloc(newcode_size);

  for ( int i = 0 ; i < inject_bytecodes_len - 1; ++i ) {
    //printf("%02x\n", co_inject[i]);
    newcode[i] = inject_bytecodes[i];
    int opcode = (int) inject_bytecodes[i];
    if ( opcode == LOAD_CONST ) {
      // Indices for constants need to be incremented by the size of the 
      // const tuple in co_code.
      newcode[i+1] = (unsigned char) inject_bytecodes[i+1] + PyTuple_Size(fn->co_consts);
      // Does the second argument to LOAD_CONST get used? For now, just copy it.
      newcode[i+2] = (unsigned char) inject_bytecodes[i+2]; 
      i += 2;
    } else if (opcode == STORE_NAME ) {
      newcode[i+1] = inject_bytecodes[i+1] + PyTuple_Size(fn->co_names);
      // Again, it's unclear if the second argument is used. Just copy for now.
      newcode[i+2] = inject_bytecodes[i+2]; 
      i += 2;
    }
    // Add in bytecodes for the code we want to be wrapped.
    for ( int i = 0; i < fn_bytecodes_len ; ++i ){
      newcode[i + inject_bytecodes_len - 1] = fn_bytecodes[i];
    }
  }
  bytecodes = newcode;
  return newcode_size;
}


static PyObject* prefixinject(PyObject *self, PyObject *args) {
  // create a new code object that concatenates inject and code and resets the code address
  // to point to the new code object
  PyCodeObject *inject;
  PyCodeObject *fn;

  if (PyArg_ParseTuple(args, "OO:PrefixSome_FnError", &inject, &fn)){
    int inject_ok = PyCode_Check(inject);
    int fn_ok = PyCode_Check(fn);
    if (inject_ok && fn_ok){

      PyObject 
	*newconsts = NULL,
	*newnames = NULL,
	*newvarnames = NULL,
	*newfreevars = NULL, 
	*newcellvars = NULL;
      
      merge_tuple(inject->co_consts, fn->co_consts, newconsts);
      merge_tuple(inject->co_names, fn->co_names,  newnames);
      merge_tuple(inject->co_varnames, fn->co_varnames, newvarnames);
      merge_tuple(inject->co_freevars, fn->co_freevars, newfreevars);
      merge_tuple(inject->co_cellvars, fn->co_cellvars, newcellvars);

      char *newcode = NULL;
      long newcode_size = merge_bytecodes(inject, fn, newcode);

      return (PyObject*) PyCode_New(inject->co_argcount + fn->co_argcount,
				    inject->co_nlocals + fn->co_nlocals,
				    inject->co_stacksize,
				    inject->co_flags | fn->co_flags,
				    PyString_FromStringAndSize(newcode, newcode_size),
				    newconsts,
				    newnames,
				    newvarnames,
				    newfreevars,
				    newcellvars,
				    fn->co_filename,
				    fn->co_name,
				    fn->co_firstlineno,
				    fn->co_lnotab
				    );
    }
    if (!inject_ok) printf("First argument is not a code object.\n");
    if (!fn_ok) printf("Second argument is not a code object.\n");
    return Py_None;
  } else {
    printf("Parsing failed\n");
    return NULL;
  }
}

static PyMethodDef InstrumentMethods[] = {
  {"prefixinject", (PyCFunction) prefixinject, METH_VARARGS,
   "Creates a new merged objects, so the first code argument is executed before the second in the same frame."},
  {NULL, NULL, 0, NULL}
};

static PyObject *InstrumentError;

PyMODINIT_FUNC initinstrument() { 
  PyObject *m;
  m = Py_InitModule("instrument", InstrumentMethods);
  if (m == NULL)
    return;
  InstrumentError = PyErr_NewException("instrument.err", NULL, NULL); 
  Py_INCREF(InstrumentError); 
  PyModule_AddObject(m, "error", InstrumentError); 
} 

int main(int argc, char *argv[]) {
  printf("Hello, World, Hello again and again!\n");
  Py_SetProgramName(argv[0]);
  Py_Initialize();
  initinstrument();
}
