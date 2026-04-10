# software/Makefile
#
# 管理所有测试套件的构建与运行。
# 在每个测试目录中直接使用 make test / make run 来单独测试。
#
# 要添加新测试：在 TESTS 列表中追加测试目录名，
# 并在该目录下按约定创建 Makefile（参考 coremark/Makefile）。

TESTS := functest coremark dhrystone picotest

.PHONY: all test-all sim-all clean

# 编译所有测试套件
all:
	@for t in $(TESTS); do $(MAKE) -C $$t image; done

# 用参考模拟器验证所有测试（无需 CPU RTL）
test-all:
	@for t in $(TESTS); do \
	    echo ""; echo "====== $$t ======"; \
	    $(MAKE) -C $$t test; \
	done

# 用 CPU 全仿真 + difftest 验证所有测试
sim-all:
	@for t in $(TESTS); do \
	    echo ""; echo "====== $$t ======"; \
	    $(MAKE) -C $$t run; \
	done

clean:
	@for t in $(TESTS); do $(MAKE) -C $$t clean; done
	$(MAKE) -C base-port clean-all
