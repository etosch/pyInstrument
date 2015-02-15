#include "Python.h"
#include "opcode.h"
#include "assert.h"
#include "stdint.h"

static void merge_tuple(PyObject *attr1, PyObject *attr2, PyObject **target) {
  // Tuple contents of the first object are placed *AFTER* the second object.
  const long attr1_size = PyTuple_Size(attr1);  
  const long attr2_size = PyTuple_Size(attr2);	 
  *target = PyTuple_New(attr1_size + attr2_size); 

  int i = 0;
  PyObject *item = NULL;

  for ( ; i < attr2_size ; ++i ) {
    item = PyTuple_GetItem(attr2, i);				
    Py_INCREF(item);	
    PyTuple_SetItem(*target, i, item);
  }		

  for (i = 0 ; i < attr1_size ; ++i ) {
    item = PyTuple_GetItem(attr1, i);
    Py_INCREF(item);
    PyTuple_SetItem(*target, i + attr2_size, item);
  } 
}

static int is_argument(char *name, int *new_index) {
  if (name[0] == '_' && name[1] == '_') {
    int i = 2;
    while (name[i] != '_') ++i;
    name[i] = '\0';
    *new_index = atoi(&name[2]) - 1;
    name[i] = '_';
    printf("\nReferring to argument index %d\n", *new_index);
    return 1;
  } 
  return 0;
}

static long merge_bytecodes(PyCodeObject *inject, PyCodeObject *fn, char **bytecodes) {

  char *inject_bytecodes;
  char *fn_bytecodes;
  Py_ssize_t inject_bytecodes_len = 0;
  Py_ssize_t fn_bytecodes_len = 0;
  
  PyString_AsStringAndSize(inject->co_code, (char **)&inject_bytecodes, &inject_bytecodes_len);
  PyString_AsStringAndSize(fn->co_code, (char **)&fn_bytecodes, &fn_bytecodes_len);
  // This is the upper bound on the size of the new bytecode list.
  Py_ssize_t newcode_size = inject_bytecodes_len + fn_bytecodes_len;

  printf("newcode_size: %ld\n", newcode_size);
  char *newcode = malloc(newcode_size);
  
  // Skip last byte code that returns.
  printf("%zd bytecodes\n", inject_bytecodes_len);
  for ( int i = 0 ; i < inject_bytecodes_len ; ++i ) {
    printf("bytecode[%d]: %02x\n", i, inject_bytecodes[i]);
    newcode[i] = inject_bytecodes[i];
    int opcode = (int) inject_bytecodes[i];

    switch (opcode) {
      
    case LOAD_CONST: {
      // Indices for constants need to be incremented by the size of the
      // const tuple in fn.
      newcode[i+1] = (unsigned char) inject_bytecodes[i+1] + PyTuple_Size(fn->co_consts);
      // Does the second argument to LOAD_CONST get used? For now, just copy it.
      newcode[i+2] = (unsigned char) inject_bytecodes[i+2];
      i += 2;
      break;
    }

    case STORE_NAME: {
      // Indices for names need to be incremented by the size of the
      // names tuple in fn.
      newcode[i+1] = (unsigned char) inject_bytecodes[i+1] + PyTuple_Size(fn->co_names);
      // Does the second argument to STORE_NAME get used? For now, just copy it.
      newcode[i+2] = (unsigned char) inject_bytecodes[i+2];
      i += 2;
      break;
    }

    case LOAD_NAME: {
      // Check if we are referring to the captured arguments.
      // 1. Get the index of the name we want to load.
      int index = (int) inject_bytecodes[i+1];
      printf("\nindex: %d ", index);
      // 2. Get the name.
      PyObject *nameObj = PyTuple_GetItem(inject->co_names, index);
      char *name = PyString_AsString(nameObj);
      printf("\nname: %s ", name);
      // 3. See if it has our special format
      int new_index;
      if (is_argument(name, &new_index)) {
	// 4a. Point to the appropriate argument in fn's co_varnames. The string is a decimal
	// representation of the argument's index. It needs to be decremented (since it starts
	// counting at 1) and cast back to char.
	// First note that LOAD_NAME will load from co_names. Change this to LOAD_FAST.
	newcode[i] = (char) LOAD_FAST;
	newcode[i+1] = (char) new_index;
	printf("\nLoading argument references (%s for %s)\n", 
	       name,
	       PyString_AsString(PyTuple_GetItem(fn->co_varnames, new_index)));
      } else {
	// 4b. Do the usual thing.
	newcode[i+1] = (unsigned char) inject_bytecodes[i+1] + PyTuple_Size(fn->co_names);
      }
      newcode[i+2] = (unsigned char) inject_bytecodes[i+2];  
      i += 2;
      break;
    }

    case RETURN_VALUE: {
      // Replace this with a call to pop the last thing on the stack, which was something 
      // that was intended to be a return value.
      printf("In return value.\n");
      newcode[i] = POP_TOP;
      break;
    }
    } //switch
  }
  printf("Done with inject bytecodes\n");
  // Add in bytecodes for the code we want to be wrapped.
  for ( int i = 0; i < fn_bytecodes_len ; ++i ){
    newcode[i + inject_bytecodes_len] = fn_bytecodes[i];
  }

  *bytecodes = newcode;
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
      
      merge_tuple(inject->co_consts, fn->co_consts, &newconsts); 
      merge_tuple(inject->co_names, fn->co_names, &newnames); 
      merge_tuple(inject->co_varnames, fn->co_varnames, &newvarnames); 
      merge_tuple(inject->co_freevars, fn->co_freevars, &newfreevars); 
      merge_tuple(inject->co_cellvars, fn->co_cellvars, &newcellvars);

      char *newcode = NULL; 
      long newcode_size = merge_bytecodes(inject, fn, &newcode); 

      fn->co_argcount += inject->co_argcount;
      fn->co_nlocals += inject->co_nlocals;
      fn->co_stacksize += inject->co_stacksize;
      fn->co_flags |= inject->co_flags;
      fn->co_code = PyString_FromStringAndSize(newcode, newcode_size);
      fn->co_consts = newconsts;
      fn->co_names = newnames;
      fn->co_varnames = newvarnames;
      fn->co_freevars = newfreevars;
      fn->co_cellvars = newcellvars;
    }
    if (!inject_ok) printf("First argument is not a code object.\n");
    if (!fn_ok) printf("Second argument is not a code object.\n");
  } else {
    printf("Parsing failed\n");
  }
  return Py_None;
}

static PyMethodDef InstrumentMethods[] = {
  {"prefixinject", (PyCFunction) prefixinject, METH_VARARGS,
   "Creates a new merged objects, so the first code argument is executed before the second in the same frame."},
  {NULL, NULL, 0, NULL}
};

static PyObject *InstrumentError;

PyMODINIT_FUNC initinstrument(void) { 
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
