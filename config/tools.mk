
# use tools based on target
ifneq (,$(findstring sanitize,$(MAKECMDGOALS)))
    SANITIZE ?= 1
endif
ifneq (,$(findstring coverage,$(MAKECMDGOALS)))
    COVERAGE ?= 1
endif
ifneq (,$(findstring analyze,$(MAKECMDGOALS)))
    ANALYZE  ?= 1
endif


# default test tools
ifneq (,$(findstring test,$(MAKECMDGOALS)))
    COVERAGE ?= 1
endif

# default demo tools
#ifneq (,$(findstring demo,$(MAKECMDGOALS)))
#    SANITIZE ?= 1
#    COVERAGE ?= 1
#endif


ifeq ($(COVERAGE),1)
 include $(MAKE_USER_MODULES)/tool.coverage.mk
endif
ifeq ($(SANITIZE),1)
 include $(MAKE_USER_MODULES)/tool.sanitize.mk
endif
ifeq ($(ANALYZE),1)
 include $(MAKE_USER_MODULES)/tool.analyze.mk
endif

