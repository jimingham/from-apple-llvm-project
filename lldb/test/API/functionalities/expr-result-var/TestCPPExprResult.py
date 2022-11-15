"""
Test the reuse of  C++ result variables, particularly making sure
that the dynamic typing is preserved.
"""



import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestCPPResultVariables(TestBase):

    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        TestBase.setUp(self)
        self.main_source_file = lldb.SBFileSpec("two-bases.cpp")
        
    def check_dereference(self, result_varname, expr_options):
        deref_expr = "*{0}".format(result_varname)
        deref_children=[ValueCheck(name="Base_1", value=""),
                        ValueCheck(name="Base_2", value=""),
                        ValueCheck(name="derived_int", value="1000")]
        result_var_deref = self.expect_expr(deref_expr, result_type="Derived",
                                            result_children = deref_children, options=expr_options) 

        direct_access_expr = "{0}->derived_int".format(result_varname)
        self.expect_expr(direct_access_expr, result_type="int", result_value="1000")
        
    def test_dynamic_results(self):
        """Test that when we uses a result variable in a subsequent expression it
           uses the dynamic value - if that was requested when the result variable was made."""
        self.build()
        (target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(self,
                                    "Set a breakpoint here", self.main_source_file)

        frame = thread.GetFrameAtIndex(0)
        expr_options = lldb.SBExpressionOptions()
        expr_options.SetFetchDynamicValue(lldb.eDynamicDontRunTarget)
        base_1_ptr = self.expect_expr("base_1_ptr", result_type="Derived *", options=expr_options)
        result_varname = base_1_ptr.GetName()
        self.check_dereference(result_varname, expr_options)
        
        # FIXME: We don't present the dynamic type of expr result references at all???
        # 'frame var base_1_ref' does present the dynamic value however, so this is wrong.
        # Now do the same thing with a Base_1 reference:
        #base_1_ref = self.expect_expr("base_1_ref", result_type="Derived &",
        #                               result_children = deref_children, options=expr_options) 

        #result_varname = base_1_ref.GetName()
        #direct_access_expr = "{0}.derived_int".format(result_varname)
        #self.expect_expr(direct_access_expr, result_type="int", result_value="1000")

        # Now check the second of the multiply inherited bases, this one will have an offset_to_top
        # that we need to calculate:
        base_2_ptr = self.expect_expr("base_2_ptr", result_type="Derived *", options=expr_options)
        result_varname = base_2_ptr.GetName()
        self.check_dereference(result_varname, expr_options)

        # Now make an expression variable that records the dynamic value and make sure
        # that also preserves its dynamic value:
        result_varname = "$base_1_var"
        base_1_var = frame.EvaluateExpression("Base_1 *{0} = base_1_ptr".format(result_varname), expr_options)
        self.check_dereference(result_varname, expr_options)


