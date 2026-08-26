/* Deliberately lacks YMGUI_Plugin_Get; discovery must ignore this library. */
int plugin_host_unrelated_library(void)
{
	return 42;
}
